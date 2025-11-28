// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_library_loader.h"

#include <android/trace.h>
#include <jni.h>
#include <sys/system_properties.h>

#include <memory>
#include <string>
#include <utility>

#include "base/android/android_info.h"
#include "base/android/base_jni_init.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/logging/logging_settings.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time_delta_from_string.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/perfetto_platform.h"
#include "build/build_config.h"
#include "components/cronet/android/cronet_base_feature.h"
#include "components/cronet/android/cronet_jni_registration_generated.h"
#include "components/cronet/android/proto/base_feature_overrides.pb.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/version.h"
#include "net/android/network_change_notifier_delegate_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/trace_net_log_observer.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/zlib/zlib.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_jni_headers/CronetLibraryLoader_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace cronet {
namespace {

// This feature flag can be used to make Cronet log a message from native code
// on library initialization. This is useful for testing that the Cronet
// base::Feature integration works.
BASE_FEATURE(kLogMe, "CronetLogMe", base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<std::string> kLogMeMessage{&kLogMe, "message", ""};

// SingleThreadTaskExecutor on the init thread, which is where objects that
// receive Java notifications generally live.
base::SingleThreadTaskExecutor* g_init_task_executor = nullptr;

std::unique_ptr<net::NetworkChangeNotifier> g_network_change_notifier;
base::WaitableEvent g_init_thread_init_done(
    base::WaitableEvent::ResetPolicy::MANUAL,
    base::WaitableEvent::InitialState::NOT_SIGNALED);

std::optional<net::NetLogCaptureMode> g_trace_net_log_capture_mode;

::org::chromium::net::httpflags::BaseFeatureOverrides GetBaseFeatureOverrides(
    JNIEnv* env) {
  const auto serializedProto =
      cronet::Java_CronetLibraryLoader_getBaseFeatureOverrides(env);
  CHECK(serializedProto);

  const auto serializedProtoSize =
      base::android::SafeGetArrayLength(env, serializedProto);
  ::org::chromium::net::httpflags::BaseFeatureOverrides overrides;
  void* const serializedProtoArray =
      env->GetPrimitiveArrayCritical(serializedProto.obj(), /*isCopy=*/nullptr);
  CHECK(serializedProtoArray != nullptr);
  CHECK(overrides.ParseFromArray(serializedProtoArray, serializedProtoSize));
  env->ReleasePrimitiveArrayCritical(serializedProto.obj(),
                                     serializedProtoArray, JNI_ABORT);
  return overrides;
}

void InitializePerfetto() {
  // This logic is inspired by
  // tracing::PerfettoTracedProcess::MaybeCreateInstance(), which is how
  // Perfetto is initialized in other Chromium products such as Clank and
  // WebView. We could, however, diverge if we have a reason to.
  static base::NoDestructor<base::tracing::PerfettoPlatform> perfetto_platform(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
      base::tracing::PerfettoPlatform::Options{
          // Use our own producer name prefix so that our traces can be
          // filtered separately from other embedded Chromium code, especially
          // WebView.
          .process_name_prefix = "cronet-"});

  perfetto::TracingInitArgs tracing_init_args;
  tracing_init_args.backends = perfetto::kSystemBackend;
  tracing_init_args.platform = perfetto_platform.get();
  tracing_init_args.enable_system_consumer = false;
  perfetto::Tracing::Initialize(tracing_init_args);

  base::TrackEvent::Register();

  // Perfetto initializes asynchronously in a background thread.  Unfortunately,
  // trace events logged while Perfetto is still initializing are just dropped
  // on the floor. This means that events logged within a brief window
  // (typically about 3 milliseconds) after initialization are lost. See
  // https://crbug.com/324031921. For this reason, it is probably a good idea to
  // prefer Android ATrace APIs to Perfetto APIs for events that are likely to
  // get lost in this way. Nevertheless, we provide a workaround in the form
  // of this system property in case the user is willing to pay an init latency
  // penalty in return for a higher likelihood of seeing early events, which can
  // be useful when e.g. tracing unit tests.
  //
  // Example usage:
  //   adb shell setprop debug.cronet.init_trace_sleep 10ms
  constexpr char trace_delay_system_property_name[] =
      "debug.cronet.init_trace_sleep";
  if (const prop_info* const trace_delay_prop_info =
          __system_property_find(trace_delay_system_property_name);
      trace_delay_prop_info != nullptr) {
    std::array<char, PROP_VALUE_MAX> trace_delay_buffer;
    const std::string_view trace_delay_str(
        trace_delay_buffer.data(),
        __system_property_read(trace_delay_prop_info, nullptr,
                               trace_delay_buffer.data()));
    const auto trace_delay = base::TimeDeltaFromString(trace_delay_str);
    if (!trace_delay.has_value()) {
      LOG(WARNING) << "Invalid value for system property "
                   << trace_delay_system_property_name;
    } else {
      ATrace_beginSection(absl::StrFormat("Sleeping %s for Perfetto",
                                          absl::FormatStreamed(*trace_delay))
                              .c_str());
      base::PlatformThread::Sleep(*trace_delay);
      ATrace_endSection();
    }
  }
}

}  // namespace

static void JNI_CronetLibraryLoader_NativeInit(JNIEnv* env,
                                               jboolean initializePerfetto) {
  logging::InitLogging(logging::LoggingSettings());

  if (!base::ThreadPoolInstance::Get()) {
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Cronet");
  }

  if (initializePerfetto) {
    ATrace_beginSection("CronetLibraryLoader_NativeInit initializing Perfetto");
    InitializePerfetto();
    ATrace_endSection();
  }

  ApplyBaseFeatureOverrides(GetBaseFeatureOverrides(env));

  if (base::FeatureList::IsEnabled(kLogMe)) {
    LOG(/* Bypass log spam warning regex */ INFO)
        << "CronetLogMe feature flag set, logging as instructed. Message: "
        << kLogMeMessage.Get();
  }
}

bool OnInitThread() {
  DCHECK(g_init_task_executor);
  return g_init_task_executor->task_runner()->RunsTasksInCurrentSequence();
}

// Checks the available version of JNI. Also, caches Java reflection artifacts.
jint CronetOnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!RegisterNatives(env)) {
    return -1;
  }
  if (!base::android::OnJNIOnLoadInit())
    return -1;
  return JNI_VERSION_1_6;
}

void CronetOnUnLoad(JavaVM* jvm, void* reserved) {
  if (base::ThreadPoolInstance::Get())
    base::ThreadPoolInstance::Get()->Shutdown();

  base::android::LibraryLoaderExitHook();
}

static void JNI_CronetLibraryLoader_CronetInitOnInitThread(
    JNIEnv* env,
    net::NetLogCaptureMode trace_net_log_capture_mode) {
  // Initialize SingleThreadTaskExecutor for init thread.
  DCHECK(!base::CurrentThread::IsSet());
  DCHECK(!g_init_task_executor);
  g_init_task_executor =
      new base::SingleThreadTaskExecutor(base::MessagePumpType::JAVA);

  static base::NoDestructor<net::TraceNetLogObserver> trace_net_log_observer(
      net::TraceNetLogObserver::Options{
          .capture_mode = trace_net_log_capture_mode,
          .use_sensitive_category = trace_net_log_capture_mode !=
                                    net::NetLogCaptureMode::kHeavilyRedacted,
          // TODO(https://crbug.com/410018349): it would be nice to have one
          // TraceNetLogObserver per CronetEngine so that each engine has its
          // own root track, if that's possible?
          .root_track_name = "Cronet NetLog",
          .verbose = true,
      });
  CHECK(!g_trace_net_log_capture_mode.has_value());
  g_trace_net_log_capture_mode = trace_net_log_capture_mode;
  // Note we do this on the init thread, as opposed to a user thread, because
  // this eventually calls
  // base::trace_event::TraceLog::AddAsyncEnabledStateObserver(), which
  // schedules its callbacks on the sequenced task runner it was called from.
  trace_net_log_observer->WatchForTraceStart(net::NetLog::Get());

  DCHECK(!g_network_change_notifier);

  if (!net::NetworkChangeNotifier::GetFactory()) {
    net::NetworkChangeNotifier::SetFactory(
        new net::NetworkChangeNotifierFactoryAndroid(
            net::NetworkChangeNotifierDelegateAndroid::ForceUpdateNetworkState::
                kDisabled));
  }
  g_network_change_notifier = net::NetworkChangeNotifier::CreateIfNeeded();
  DCHECK(g_network_change_notifier);

  g_init_thread_init_done.Signal();
}

static net::NetLogCaptureMode
JNI_CronetLibraryLoader_GetTraceNetLogCaptureModeForTesting(JNIEnv* env) {
  return g_trace_net_log_capture_mode.value();
}

static ScopedJavaLocalRef<jstring> JNI_CronetLibraryLoader_GetCronetVersion(
    JNIEnv* env) {
#if defined(ARCH_CPU_ARM64)
  // Attempt to avoid crashes on some ARM64 Marshmallow devices by
  // prompting zlib ARM feature detection early on. https://crbug.com/853725
  if (base::android::android_info::sdk_int() ==
      base::android::android_info::SDK_VERSION_MARSHMALLOW) {
    crc32(0, Z_NULL, 0);
  }
#endif
  return base::android::ConvertUTF8ToJavaString(env, CRONET_VERSION);
}

static void JNI_CronetLibraryLoader_SetMinLogLevel(JNIEnv* env,
                                                   jint jlog_level) {
  logging::SetMinLogLevel(jlog_level);
}

void PostTaskToInitThread(const base::Location& posted_from,
                          base::OnceClosure task) {
  g_init_thread_init_done.Wait();
  g_init_task_executor->task_runner()->PostTask(posted_from, std::move(task));
}

void EnsureInitialized() {
  if (g_init_task_executor) {
    // Ensure that init is done on the init thread.
    g_init_thread_init_done.Wait();
    return;
  }

  // The initialization can only be done once, so static |s_run_once| variable
  // is used to do it in the constructor.
  static class RunOnce {
   public:
    RunOnce() {
      JNIEnv* env = base::android::AttachCurrentThread();
      // Ensure initialized from Java side to properly create Init thread.
      cronet::Java_CronetLibraryLoader_ensureInitializedFromNative(env);
    }
  } s_run_once;
}

std::unique_ptr<net::ProxyConfigService> CreateProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  // Note: CreateSystemProxyConfigService internally assumes that
  // base::SingleThreadTaskRunner::GetCurrentDefault() == JNI communication
  // thread.
  std::unique_ptr<net::ProxyConfigService> service =
      net::ProxyConfigService::CreateSystemProxyConfigService(io_task_runner);
  // If a PAC URL is present, ignore it and use the address and port of
  // Android system's local HTTP proxy server. See: crbug.com/432539.
  // TODO(csharrison) Architect the wrapper better so we don't need to cast for
  // android ProxyConfigServices.
  net::ProxyConfigServiceAndroid* android_proxy_config_service =
      static_cast<net::ProxyConfigServiceAndroid*>(service.get());
  android_proxy_config_service->set_exclude_pac_url(true);
  return service;
}

// Creates a proxy resolution service appropriate for this platform.
std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::NetLog* net_log) {
  // Android provides a local HTTP proxy server that handles proxying when a PAC
  // URL is present. Create a proxy service without a resolver and rely on this
  // local HTTP proxy. See: crbug.com/432539.
  return net::ConfiguredProxyResolutionService::CreateWithoutProxyResolver(
      std::move(proxy_config_service),
      /*host_resolver_for_override_rules=*/nullptr, net_log);
}

// Creates default User-Agent request value, combining optional
// |partial_user_agent| with system-dependent values.
std::string CreateDefaultUserAgent(const std::string& partial_user_agent) {
  // Cronet global state must be initialized to include application info
  // into default user agent
  cronet::EnsureInitialized();

  JNIEnv* env = base::android::AttachCurrentThread();
  std::string user_agent = base::android::ConvertJavaStringToUTF8(
      cronet::Java_CronetLibraryLoader_getDefaultUserAgent(env));
  if (!partial_user_agent.empty())
    user_agent.insert(user_agent.size() - 1, "; " + partial_user_agent);
  return user_agent;
}

void SetNetworkThreadPriorityOnNetworkThread(double priority) {
  int priority_int = priority;
  DCHECK_LE(priority_int, 19);
  DCHECK_GE(priority_int, -20);
  if (priority_int >= -20 && priority_int <= 19) {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetLibraryLoader_setNetworkThreadPriorityOnNetworkThread(
        env, priority_int);
  }
}

}  // namespace cronet

DEFINE_JNI(CronetLibraryLoader)
