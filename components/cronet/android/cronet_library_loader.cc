// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_library_loader.h"

#include <android/trace.h>
#include <jni.h>
#include <sys/system_properties.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/base_jni_init.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/check_op.h"
#include "base/containers/span.h"
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
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/mlkem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/zlib/zlib.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_jni_headers/CronetLibraryLoader_jni.h"

using base::android::JavaRef;
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

static void JNI_CronetLibraryLoader_ExecuteSelfTests(JNIEnv* env) {
  // This function performs various cryptographic operations to trigger
  // BoringSSL's self-tests.
  //
  // - RSA: Verification is performed using pre-computed public keys and
  //   signatures to avoid the overhead of a full signing operation. RSA's
  //   signing self-test is not triggered here, as client-side signing is a
  //   rare operation and is deferred until needed.
  // - ML-KEM: Key-generation, encapsulation, and decapsulation are all
  //   exercised.
  // - ECC: Verification is performed using -repcomputed public keys and
  //   signatures to avoid the overhead of a full signing operation.
  //
  // This is the SHA-256 hash of an empty message.
  static constexpr std::array<uint8_t, 32> msg_hash = {
      0x6E, 0x34, 0x0B, 0x9C, 0xFF, 0xB3, 0x7A, 0x98, 0x9C, 0xA5, 0x44,
      0xE6, 0xBB, 0x78, 0x0A, 0x2C, 0x78, 0x90, 0x1D, 0x3F, 0xB3, 0x37,
      0x38, 0x76, 0x85, 0x11, 0xA3, 0x06, 0x17, 0xAF, 0xA0, 0x1D};
  // RSA
  {
    static constexpr std::array<uint8_t, 270> public_key_bytes = {
        0x30, 0x82, 0x01, 0x0A, 0x02, 0x82, 0x01, 0x01, 0x00, 0xED, 0x9C, 0x8C,
        0xCC, 0x99, 0xBC, 0xC3, 0x6C, 0x4D, 0x3A, 0x4C, 0x19, 0x41, 0x26, 0x97,
        0xAD, 0x0B, 0x00, 0x5B, 0x95, 0x8A, 0x13, 0xDA, 0xF2, 0x12, 0xC3, 0x96,
        0x24, 0x15, 0xE2, 0x1D, 0xAE, 0x88, 0x0C, 0x53, 0x38, 0xA8, 0x59, 0x5E,
        0x4A, 0x5C, 0x66, 0xD0, 0x81, 0x3D, 0x0D, 0x6B, 0x16, 0xA8, 0x2E, 0x2F,
        0xB1, 0x0A, 0xE2, 0xA2, 0x78, 0xBF, 0x08, 0xCA, 0x6B, 0xDC, 0x18, 0xBC,
        0x62, 0x13, 0xC9, 0x53, 0xC9, 0x8E, 0x8A, 0x76, 0x00, 0x2C, 0xD5, 0x35,
        0x63, 0x78, 0x9B, 0x3A, 0xFF, 0xC3, 0x90, 0xEA, 0x24, 0x60, 0xE1, 0x2F,
        0x0E, 0x43, 0xFD, 0x0A, 0xBF, 0x14, 0xFF, 0xDA, 0x2E, 0x52, 0xF3, 0xE9,
        0xC7, 0x22, 0x99, 0x29, 0xF9, 0xB9, 0x9E, 0x3D, 0x8D, 0x44, 0xA4, 0x82,
        0x09, 0xED, 0x7B, 0x54, 0x53, 0xE7, 0x3A, 0xAC, 0x55, 0x1F, 0x92, 0xA7,
        0x4C, 0xB1, 0x10, 0x22, 0x2F, 0xEE, 0x4E, 0x62, 0x65, 0x34, 0x6B, 0x52,
        0x06, 0x41, 0xA2, 0x03, 0x0D, 0xA5, 0x19, 0xCE, 0x36, 0xCE, 0x23, 0xF0,
        0xA8, 0x0C, 0x58, 0x5B, 0xE0, 0xDF, 0x7C, 0xBC, 0x83, 0x28, 0x1D, 0xAC,
        0x0E, 0x80, 0xB3, 0x43, 0xDE, 0x3A, 0xB9, 0x98, 0x03, 0x47, 0x86, 0x33,
        0x89, 0xC8, 0x3F, 0x5D, 0xDD, 0x5C, 0x86, 0x25, 0xCA, 0x03, 0x09, 0x01,
        0xE3, 0xB9, 0x09, 0xAB, 0x75, 0xD7, 0x69, 0x81, 0x14, 0xEA, 0xB5, 0x2E,
        0x2C, 0xC4, 0xD8, 0x64, 0xCB, 0x1C, 0x70, 0x16, 0x59, 0x23, 0xC6, 0x54,
        0x16, 0xAF, 0x68, 0x25, 0xBD, 0x11, 0x17, 0x49, 0x34, 0x94, 0x22, 0x17,
        0xD9, 0x14, 0x7F, 0x41, 0x6D, 0x57, 0xFE, 0xED, 0xEF, 0xEB, 0x9C, 0x72,
        0x5D, 0xD2, 0x74, 0x89, 0x1E, 0xA2, 0x27, 0x1F, 0x5B, 0x85, 0x54, 0x54,
        0xBA, 0x4C, 0x9F, 0x25, 0x6D, 0xDA, 0xAF, 0x7D, 0x68, 0x77, 0x36, 0x1E,
        0xCF, 0x02, 0x03, 0x01, 0x00, 0x01};
    static constexpr std::array<uint8_t, 256> signature_bytes = {
        0xDB, 0x14, 0xF6, 0x72, 0x68, 0x02, 0x38, 0xD0, 0x4E, 0x0B, 0x5D, 0xC5,
        0xCE, 0x52, 0x59, 0x7C, 0x6D, 0xB5, 0xA5, 0x38, 0xA9, 0x3C, 0x53, 0x8E,
        0xF3, 0x51, 0x17, 0xE1, 0x96, 0xBA, 0x1E, 0x97, 0xE7, 0xCB, 0x38, 0x4C,
        0xA0, 0xC1, 0x09, 0x84, 0xB7, 0x1C, 0x74, 0x2A, 0x3D, 0xB4, 0x44, 0x8B,
        0x18, 0x85, 0x61, 0xF4, 0xF2, 0xF1, 0xD2, 0x60, 0x0C, 0x99, 0xEF, 0xF2,
        0x8A, 0x2E, 0x74, 0x66, 0x47, 0x87, 0x40, 0xA6, 0xA1, 0xBB, 0xA5, 0xBD,
        0x7E, 0x2B, 0x1C, 0x95, 0xDF, 0xE4, 0x59, 0x38, 0x27, 0x31, 0xA7, 0xFF,
        0x0C, 0x4B, 0x4E, 0x07, 0x45, 0x16, 0xE6, 0x15, 0x94, 0xA4, 0x80, 0x90,
        0x51, 0x73, 0x96, 0xF7, 0x48, 0x36, 0x53, 0x7C, 0xEC, 0xD8, 0x0C, 0xAD,
        0x71, 0xD1, 0xAD, 0xDC, 0x8E, 0x1D, 0x4E, 0xE9, 0x72, 0x36, 0xA0, 0x9A,
        0x59, 0xAE, 0x23, 0x87, 0xC1, 0xD0, 0x82, 0x6D, 0x4A, 0x30, 0x36, 0xF1,
        0xC9, 0x36, 0x6B, 0xED, 0x81, 0x5C, 0x38, 0xFD, 0x1D, 0x50, 0x5C, 0x74,
        0x53, 0x10, 0x3D, 0xA8, 0x06, 0x54, 0xE1, 0xFF, 0x01, 0x7E, 0xEF, 0xD7,
        0xF2, 0x70, 0xA8, 0x7F, 0x25, 0xA2, 0xC5, 0x1F, 0x28, 0xA0, 0x45, 0x0A,
        0x97, 0x1F, 0xB9, 0x06, 0xF1, 0x75, 0x49, 0x60, 0x3A, 0x79, 0x33, 0x61,
        0xC3, 0xFA, 0xF8, 0x67, 0x1E, 0xBF, 0xC2, 0xD9, 0xA9, 0x49, 0x08, 0x8E,
        0x21, 0xE3, 0xC1, 0x6C, 0x8C, 0x5F, 0x65, 0xBE, 0xE3, 0x3A, 0x18, 0x3A,
        0x41, 0x9F, 0xCC, 0x9C, 0x95, 0x59, 0x99, 0x8A, 0xCD, 0xB5, 0x80, 0x3C,
        0xE0, 0x44, 0x59, 0x87, 0x0B, 0x15, 0x1F, 0x87, 0x1D, 0xA5, 0xA2, 0xFA,
        0x1B, 0xA7, 0x9B, 0xC5, 0x9D, 0x7B, 0xC0, 0xA8, 0x96, 0xBF, 0x8E, 0x76,
        0x34, 0x0A, 0xB4, 0xE8, 0xA2, 0xD7, 0x07, 0xD8, 0xF8, 0x21, 0xA4, 0x5F,
        0x2F, 0x36, 0x4D, 0x63};

    bssl::UniquePtr<RSA> rsa(RSA_public_key_from_bytes(
        public_key_bytes.data(), public_key_bytes.size()));
    CHECK(rsa);
    CHECK(RSA_verify(NID_sha256, msg_hash.data(), msg_hash.size(),
                     signature_bytes.data(), signature_bytes.size(),
                     rsa.get()));
  }

  // MLKEM
  {
    struct MLKEM768_public_key pub;
    struct MLKEM768_private_key priv;
    std::array<uint8_t, MLKEM768_PUBLIC_KEY_BYTES> encoded_pub;
    MLKEM768_generate_key(encoded_pub.data(), nullptr, &priv);
    MLKEM768_public_from_private(&pub, &priv);

    std::array<uint8_t, MLKEM768_CIPHERTEXT_BYTES> ct;
    std::array<uint8_t, MLKEM_SHARED_SECRET_BYTES> ss_enc;
    std::array<uint8_t, MLKEM_SHARED_SECRET_BYTES> ss_dec;

    MLKEM768_encap(ct.data(), ss_enc.data(), &pub);
    CHECK(MLKEM768_decap(ss_dec.data(), ct.data(), ct.size(), &priv));
    CHECK(base::span(ss_enc) == base::span(ss_dec));
  }

  // ECC
  {
    static constexpr std::array<uint8_t, 65> public_key_bytes = {
        0x04, 0x36, 0x44, 0xCB, 0x80, 0xAC, 0x54, 0xD2, 0xF4, 0x20, 0xD5,
        0x69, 0x46, 0x9C, 0x3A, 0xAA, 0x1D, 0x24, 0x33, 0x41, 0x04, 0x5A,
        0x1F, 0x36, 0x14, 0x98, 0x94, 0xC9, 0x81, 0x56, 0xBA, 0x1A, 0xD3,
        0x30, 0x27, 0x06, 0x36, 0x27, 0x56, 0xD6, 0x33, 0xAB, 0x18, 0xC6,
        0x6F, 0x7C, 0xB4, 0x2A, 0xE0, 0xB5, 0x9E, 0xEC, 0xA2, 0x44, 0xFD,
        0x7D, 0x82, 0xF5, 0x76, 0x57, 0x30, 0xE6, 0x01, 0x0F, 0x84};
    // This is msg_hash signed by the private key.
    static constexpr std::array<uint8_t, 70> signature_bytes = {
        0x30, 0x44, 0x02, 0x20, 0x63, 0xA3, 0x3B, 0x30, 0x12, 0xB9, 0x13, 0x49,
        0x1E, 0xE3, 0xA2, 0xC6, 0x27, 0xF7, 0x13, 0xCA, 0xDD, 0x0A, 0xEC, 0xBA,
        0x86, 0xB6, 0xCD, 0xAF, 0x39, 0x2E, 0xE0, 0x7E, 0xD3, 0xF2, 0xE9, 0x4A,
        0x02, 0x20, 0x12, 0xF7, 0x16, 0x4C, 0x77, 0x98, 0xB5, 0x20, 0x57, 0x80,
        0xA8, 0x11, 0x1A, 0x67, 0x8F, 0x26, 0x78, 0xCD, 0xB3, 0xE3, 0xBC, 0xE5,
        0x79, 0x9E, 0xCD, 0xCA, 0xDE, 0xA4, 0x39, 0x14, 0x8E, 0x3D};

    bssl::UniquePtr<EC_KEY> ec_key(
        EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    CHECK(ec_key);
    CHECK(EC_KEY_oct2key(ec_key.get(), public_key_bytes.data(),
                         public_key_bytes.size(), nullptr));
    CHECK(ECDSA_verify(0, msg_hash.data(), msg_hash.size(),
                       signature_bytes.data(), signature_bytes.size(),
                       ec_key.get()));
  }
}

static void JNI_CronetLibraryLoader_NativeInit(JNIEnv* env,
                                               bool initializePerfetto) {
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
int32_t CronetOnLoad(JavaVM* vm, void* reserved) {
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
  // this calls base::trace_event::TraceSessionObserverList::AddObserver(),
  // which schedules its callbacks on the sequenced task runner it was called
  // from.
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
                                                   int32_t jlog_level) {
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
