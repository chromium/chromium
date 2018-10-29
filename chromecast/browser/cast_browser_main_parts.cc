// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_main_parts.h"

#include <stddef.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chromecast/base/cast_constants.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/metrics/grouped_histogram.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/cast_memory_pressure_monitor.h"
#include "chromecast/browser/cast_net_log.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/browser/metrics/cast_metrics_prefs.h"
#include "chromecast/browser/metrics/cast_metrics_service_client.h"
#include "chromecast/browser/pref_service_helper.h"
#include "chromecast/browser/tts/tts_controller_impl.h"
#include "chromecast/browser/tts/tts_platform_stub.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/global_descriptors.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/media/base/key_systems_common.h"
#include "chromecast/media/base/media_resource_tracker.h"
#include "chromecast/media/base/video_plane_controller.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/net/connectivity_checker.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/service/cast_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_LINUX)
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <sys/prctl.h>
#endif

#if defined(OS_ANDROID)
#include "chromecast/app/android/crash_handler.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#else
#include "chromecast/net/network_change_notifier_factory_cast.h"
#endif

#if defined(OS_FUCHSIA)
#include "chromecast/net/fake_connectivity_checker.h"
#endif

#if defined(USE_AURA)
// gn check ignored on OverlayManagerCast as it's not a public ozone
// header, but is exported to allow injecting the overlay-composited
// callback.
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_display_configurator.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/graphics/cast_window_manager_aura.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"  // nogncheck
#include "ui/display/screen.h"
#else
#include "chromecast/graphics/cast_window_manager_default.h"
#endif

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/browser/extensions/api/tts/tts_extension_api.h"
#include "chromecast/browser/extensions/cast_extension_system.h"
#include "chromecast/browser/extensions/cast_extensions_browser_client.h"
#include "chromecast/browser/extensions/cast_prefs.h"
#include "chromecast/common/cast_extensions_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"  // nogncheck
#include "extensions/browser/browser_context_keyed_service_factories.h"  // nogncheck
#include "extensions/browser/extension_prefs.h"  // nogncheck
#endif

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
#include "device/bluetooth/cast/bluetooth_adapter_cast.h"
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

namespace {

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
int kSignalsToRunClosure[] = {
    SIGTERM, SIGINT,
};
// Closure to run on SIGTERM and SIGINT.
base::Closure* g_signal_closure = nullptr;
base::PlatformThreadId g_main_thread_id;

void RunClosureOnSignal(int signum) {
  if (base::PlatformThread::CurrentId() != g_main_thread_id) {
    RAW_LOG(INFO, "Received signal on non-main thread\n");
    return;
  }

  char message[48] = "Received close signal: ";
  strncat(message, sys_siglist[signum], sizeof(message) - strlen(message) - 1);
  RAW_LOG(INFO, message);

  DCHECK(g_signal_closure);
  g_signal_closure->Run();
}

void RegisterClosureOnSignal(const base::Closure& closure) {
  DCHECK(!g_signal_closure);
  DCHECK_GT(arraysize(kSignalsToRunClosure), 0U);

  // Memory leak on purpose, since |g_signal_closure| should live until
  // process exit.
  g_signal_closure = new base::Closure(closure);
  g_main_thread_id = base::PlatformThread::CurrentId();

  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = RunClosureOnSignal;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  for (int sig : kSignalsToRunClosure) {
    struct sigaction sa_old;
    if (sigaction(sig, &sa_new, &sa_old) == -1) {
      NOTREACHED();
    } else {
      DCHECK_EQ(sa_old.sa_handler, SIG_DFL);
    }
  }

  // Get the first signal to exit when the parent process dies.
  prctl(PR_SET_PDEATHSIG, kSignalsToRunClosure[0]);
}

const int kKillOnAlarmTimeoutSec = 5;  // 5 seconds

void KillOnAlarm(int signum) {
  LOG(ERROR) << "Got alarm signal for termination: " << signum;
  raise(SIGKILL);
}

void RegisterKillOnAlarm(int timeout_seconds) {
  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = KillOnAlarm;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  struct sigaction sa_old;
  if (sigaction(SIGALRM, &sa_new, &sa_old) == -1) {
    NOTREACHED();
  } else {
    DCHECK_EQ(sa_old.sa_handler, SIG_DFL);
  }

  if (alarm(timeout_seconds) > 0)
    NOTREACHED() << "Previous alarm() was cancelled";
}

void DeregisterKillOnAlarm() {
  // Explicitly cancel any outstanding alarm() calls.
  alarm(0);

  struct sigaction sa_new;
  memset(&sa_new, 0, sizeof(sa_new));
  sa_new.sa_handler = SIG_DFL;
  sigfillset(&sa_new.sa_mask);
  sa_new.sa_flags = SA_RESTART;

  struct sigaction sa_old;
  if (sigaction(SIGALRM, &sa_new, &sa_old) == -1) {
    NOTREACHED();
  } else {
    DCHECK_EQ(sa_old.sa_handler, KillOnAlarm);
  }
}

#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

}  // namespace

namespace chromecast {
namespace shell {

namespace {

struct DefaultCommandLineSwitch {
  const char* const switch_name;
  const char* const switch_value;
};

const DefaultCommandLineSwitch kDefaultSwitches[] = {
#if !defined(OS_ANDROID)
    // GPU shader disk cache disabling is largely to conserve disk space.
    {switches::kDisableGpuShaderDiskCache, ""},
    // Enable media sessions by default (even on non-Android platforms).
    {media_session::switches::kEnableInternalMediaSession, ""},
#endif
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
    {switches::kDisableGpu, ""},
#if defined(OS_ANDROID)
    {switches::kDisableFrameRateLimit, ""},
    {switches::kDisableGLDrawingForTests, ""},
    {switches::kDisableGpuCompositing, ""},
    {cc::switches::kDisableThreadedAnimation, ""},
#endif  // defined(OS_ANDROID)
#endif  // BUILDFLAG(IS_CAST_AUDIO_ONLY)
#if defined(OS_LINUX)
#if defined(ARCH_CPU_X86_FAMILY)
    // This is needed for now to enable the x11 Ozone platform to work with
    // current Linux/NVidia OpenGL drivers.
    {switches::kIgnoreGpuBlacklist, ""},
#elif defined(ARCH_CPU_ARM_FAMILY)
#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
    {switches::kEnableHardwareOverlays, "cast"},
#endif
#endif
#endif  // defined(OS_LINUX)
    // It's better to start GPU process on demand. For example, for TV platforms
    // cast starts in background and can't render until TV switches to cast
    // input.
    {switches::kDisableGpuEarlyInit, ""},
    // Enable navigator.connection API.
    // TODO(derekjchow): Remove this switch when enabled by default.
    {switches::kEnableNetworkInformationDownlinkMax, ""},
    // TODO(halliwell): Remove after fixing b/35422666.
    {switches::kEnableUseZoomForDSF, "false"},
    // TODO(halliwell): Revert after fix for b/63101386.
    {switches::kDisallowNonExactResourceReuse, ""},
    // Enable autoplay without requiring any user gesture.
    {switches::kAutoplayPolicy,
     switches::autoplay::kNoUserGestureRequiredPolicy},
    // Disable pinch zoom gesture.
    {switches::kDisablePinch, ""},
};

void AddDefaultCommandLineSwitches(base::CommandLine* command_line) {
  std::string default_command_line_flags_string =
      BUILDFLAG(DEFAULT_COMMAND_LINE_FLAGS);
  std::vector<std::string> default_command_line_flags_list =
      base::SplitString(default_command_line_flags_string, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (auto default_command_line_flag : default_command_line_flags_list) {
    std::vector<std::string> default_command_line_flag_content =
        base::SplitString(default_command_line_flag, "=", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (default_command_line_flag_content.size() == 2 &&
        !command_line->HasSwitch(default_command_line_flag_content[0])) {
      DVLOG(2) << "Set default command line switch '"
               << default_command_line_flag_content[0] << "' = '"
               << default_command_line_flag_content[1] << "'";
      command_line->AppendSwitchASCII(default_command_line_flag_content[0],
                                      default_command_line_flag_content[1]);
    }
  }

  for (const auto& default_switch : kDefaultSwitches) {
    // Don't override existing command line switch values with these defaults.
    // This could occur primarily (or only) on Android, where the command line
    // is initialized in Java first.
    std::string name(default_switch.switch_name);
    if (!command_line->HasSwitch(name)) {
      std::string value(default_switch.switch_value);
      VLOG(2) << "Set default switch '" << name << "' = '" << value << "'";
      command_line->AppendSwitchASCII(name, value);
    } else {
      VLOG(2) << "Skip setting default switch '" << name << "', already set";
    }
  }
}

}  // namespace

CastBrowserMainParts::CastBrowserMainParts(
    const content::MainFunctionParams& parameters,
    URLRequestContextFactory* url_request_context_factory,
    CastContentBrowserClient* cast_content_browser_client)
    : BrowserMainParts(),
      cast_browser_process_(new CastBrowserProcess()),
      field_trial_list_(nullptr),
      parameters_(parameters),
      cast_content_browser_client_(cast_content_browser_client),
      url_request_context_factory_(url_request_context_factory),
      net_log_(new CastNetLog()),
      media_caps_(new media::MediaCapsImpl()) {
  DCHECK(cast_content_browser_client);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  AddDefaultCommandLineSwitches(command_line);
}

CastBrowserMainParts::~CastBrowserMainParts() {
#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
  if (cast_content_browser_client_->GetMediaTaskRunner() &&
      media_pipeline_backend_manager_) {
    // Make sure that media_pipeline_backend_manager_ is destroyed after any
    // pending media thread tasks. The CastAudioOutputStream implementation
    // calls into media_pipeline_backend_manager_ when the stream is closed;
    // therefore, we must be sure that all CastAudioOutputStreams are gone
    // before destroying media_pipeline_backend_manager_. This is guaranteed
    // once the AudioManager is destroyed; the AudioManager destruction is
    // posted to the media thread in the BrowserMainLoop destructor, just before
    // the BrowserMainParts are destroyed (ie, here). Therefore, if we delete
    // the media_pipeline_backend_manager_ using DeleteSoon on the media thread,
    // it is guaranteed that the AudioManager and all AudioOutputStreams have
    // been destroyed before media_pipeline_backend_manager_ is destroyed.
    cast_content_browser_client_->GetMediaTaskRunner()->DeleteSoon(
        FROM_HERE, media_pipeline_backend_manager_.release());
  }
#endif  // BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
}

#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
media::MediaPipelineBackendManager*
CastBrowserMainParts::media_pipeline_backend_manager() {
  if (!media_pipeline_backend_manager_) {
    media_pipeline_backend_manager_ =
        std::make_unique<media::MediaPipelineBackendManager>(
            cast_content_browser_client_->GetMediaTaskRunner());
  }
  return media_pipeline_backend_manager_.get();
}
#endif  // BUILDFLAG(IS_CAST_USING_CMA_BACKEND)

media::MediaCapsImpl* CastBrowserMainParts::media_caps() {
  return media_caps_.get();
}

content::BrowserContext* CastBrowserMainParts::browser_context() {
  return cast_browser_process_->browser_context();
}

void CastBrowserMainParts::PreMainMessageLoopStart() {
  // GroupedHistograms needs to be initialized before any threads are created
  // to prevent race conditions between calls to Preregister and those threads
  // attempting to collect metrics.
  // This call must also be before NetworkChangeNotifier, as it generates
  // Net/DNS metrics.
  metrics::PreregisterAllGroupedHistograms();

#if defined(OS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#elif !defined(OS_FUCHSIA)
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryCast());
#endif  // !defined(OS_FUCHSIA)
}

void CastBrowserMainParts::PostMainMessageLoopStart() {
  // Ensure CastMetricsHelper initialized on UI thread.
  metrics::CastMetricsHelper::GetInstance();
}

void CastBrowserMainParts::ToolkitInitialized() {
#if defined(OS_LINUX)
  // Without this call, the FontConfig library gets implicitly initialized
  // on the first call to FontConfig. Since it's not safe to initialize it
  // concurrently from multiple threads, we explicitly initialize it here
  // to prevent races when there are multiple renderer's querying the library:
  // http://crbug.com/404311
  // Also, implicit initialization can cause a long delay on the first
  // rendering if the font cache has to be regenerated for some reason. Doing it
  // explicitly here helps in cases where the browser process is starting up in
  // the background (resources have not yet been granted to cast) since it
  // prevents the long delay the user would have seen on first rendering. Note
  // that future calls to FcInit() are safe no-ops per the FontConfig interface.
  FcChar8 bundle_dir[] = "/chrome/fonts/";

  FcInit();

  if (FcConfigAppFontAddDir(nullptr, bundle_dir) == FcFalse) {
    LOG(ERROR) << "Cannot load fonts from " << bundle_dir;
  }
#endif
}

int CastBrowserMainParts::PreCreateThreads() {
#if defined(OS_ANDROID)
  // GPU process is started immediately after threads are created,
  // requiring ChildProcessCrashObserver to be initialized beforehand.
  base::FilePath crash_dumps_dir;
  if (!chromecast::CrashHandler::GetCrashDumpLocation(&crash_dumps_dir)) {
    LOG(ERROR) << "Could not find crash dump location.";
  }
  crash_reporter::ChildExitObserver::Create();
  crash_reporter::ChildExitObserver::GetInstance()->RegisterClient(
      std::make_unique<crash_reporter::ChildProcessCrashObserver>(
          crash_dumps_dir, kAndroidMinidumpDescriptor));
#else
  base::FilePath home_dir;
  CHECK(base::PathService::Get(DIR_CAST_HOME, &home_dir));
  if (!base::CreateDirectory(home_dir))
    return 1;
#endif

  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
  metrics::RegisterPrefs(pref_registry.get());
  PrefProxyConfigTrackerImpl::RegisterPrefs(pref_registry.get());
  cast_browser_process_->SetPrefService(
      PrefServiceHelper::CreatePrefService(pref_registry.get()));

  // As soon as the PrefService is set, initialize the base::FeatureList, so
  // objects initialized after this point can use features from
  // base::FeatureList.
  const auto* features_dict =
      cast_browser_process_->pref_service()->GetDictionary(
          prefs::kLatestDCSFeatures);
  const auto* experiment_ids = cast_browser_process_->pref_service()->GetList(
      prefs::kActiveDCSExperiments);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  InitializeFeatureList(
      *features_dict, *experiment_ids,
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures));

#if defined(USE_AURA)
  cast_browser_process_->SetCastScreen(std::make_unique<CastScreen>());
  DCHECK(!display::Screen::GetScreen());
  display::Screen::SetScreenInstance(cast_browser_process_->cast_screen());
  cast_browser_process_->SetDisplayConfigurator(
      std::make_unique<CastDisplayConfigurator>(
          cast_browser_process_->cast_screen()));
#endif  // defined(USE_AURA)

  content::ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      kChromeResourceScheme);
  return 0;
}

void CastBrowserMainParts::PreMainMessageLoopRun() {
#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  memory_pressure_monitor_.reset(new CastMemoryPressureMonitor());

  // base::Unretained() is safe because the browser client will outlive any
  // component in the browser; this factory method will not be called after
  // the browser starts to tear down.
  device::BluetoothAdapterCast::SetFactory(base::BindRepeating(
      &CastContentBrowserClient::CreateBluetoothAdapter,
      base::Unretained(cast_browser_process_->browser_client())));
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

#if defined(OS_ANDROID)
  crash_reporter_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  crash_reporter_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CastBrowserMainParts::StartPeriodicCrashReportUpload,
                     base::Unretained(this)));
#endif  // defined(OS_ANDROID)

  cast_browser_process_->SetNetLog(net_log_.get());
  url_request_context_factory_->InitializeOnUIThread(net_log_.get());

#if defined(OS_FUCHSIA)
  // TODO(777973): Switch to using the real ConnectivityChecker once setup works
  // properly.
  LOG(WARNING) << "Using FakeConnectivityChecker.";
  cast_browser_process_->SetConnectivityChecker(new FakeConnectivityChecker());
#else
  cast_browser_process_->SetConnectivityChecker(ConnectivityChecker::Create(
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO}),
      url_request_context_factory_->GetSystemGetter()));
#endif  // defined(OS_FUCHSIA)

  cast_browser_process_->SetBrowserContext(
      std::make_unique<CastBrowserContext>(url_request_context_factory_));
  cast_browser_process_->SetMetricsServiceClient(
      std::make_unique<metrics::CastMetricsServiceClient>(
          cast_browser_process_->pref_service(),
          content::BrowserContext::GetDefaultStoragePartition(
              cast_browser_process_->browser_context())
              ->GetURLLoaderFactoryForBrowserProcess()));
  cast_browser_process_->SetRemoteDebuggingServer(
      std::make_unique<RemoteDebuggingServer>(
          cast_browser_process_->browser_client()
              ->EnableRemoteDebuggingImmediately()));

#if defined(USE_AURA) && !BUILDFLAG(IS_CAST_AUDIO_ONLY)
  // TODO(halliwell) move audio builds to use ozone_platform_cast, then can
  // simplify this by removing IS_CAST_AUDIO_ONLY condition.  Should then also
  // assert(ozone_platform_cast) in BUILD.gn where it depends on //ui/ozone.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  video_plane_controller_.reset(new media::VideoPlaneController(
      Size(display_size.width(), display_size.height()),
      cast_content_browser_client_->GetMediaTaskRunner()));
  viz::OverlayStrategyUnderlayCast::SetOverlayCompositedCallback(
      base::BindRepeating(&media::VideoPlaneController::SetGeometry,
                          base::Unretained(video_plane_controller_.get())));
#endif

#if defined(USE_AURA)
  window_manager_ = std::make_unique<CastWindowManagerAura>(
      CAST_IS_DEBUG_BUILD() ||
      GetSwitchValueBoolean(switches::kEnableInput, false));
  window_manager_->Setup();

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  cast_browser_process_->SetAccessibilityManager(
      std::make_unique<AccessibilityManager>(window_manager_.get()));
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

#else   // defined(USE_AURA)
  window_manager_ = std::make_unique<CastWindowManagerDefault>();
#endif  // defined(USE_AURA)

  cast_browser_process_->SetCastService(
      cast_browser_process_->browser_client()->CreateCastService(
          cast_browser_process_->browser_context(),
          cast_browser_process_->pref_service(),
          url_request_context_factory_->GetSystemGetter(),
          video_plane_controller_.get(), window_manager_.get()));
  cast_browser_process_->cast_service()->Initialize();

#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
  cast_content_browser_client_->media_resource_tracker()->InitializeMediaLib();
#endif
  ::media::InitializeMediaLibrary();
  media_caps_->Initialize();

  cast_browser_process_->SetTtsController(std::make_unique<TtsControllerImpl>(
      std::make_unique<TtsPlatformImplStub>()));

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  user_pref_service_ = extensions::cast_prefs::CreateUserPrefService(
      cast_browser_process_->browser_context());

  extensions_client_ = std::make_unique<extensions::CastExtensionsClient>();
  extensions::ExtensionsClient::Set(extensions_client_.get());

  extensions_browser_client_ =
      std::make_unique<extensions::CastExtensionsBrowserClient>(
          cast_browser_process_->browser_context(), user_pref_service_.get());
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  extensions::EnsureBrowserContextKeyedServiceFactoriesBuilt();

  extensions::CastExtensionSystem* extension_system =
      static_cast<extensions::CastExtensionSystem*>(
          extensions::ExtensionSystem::Get(
              cast_browser_process_->browser_context()));

  extension_system->InitForRegularProfile(true);
  extension_system->Init();

  extensions::ExtensionPrefs::Get(cast_browser_process_->browser_context());

  // Force TTS to be available. It's lazy and this makes it eager.
  // TODO(rdaum): There has to be a better way.
  extensions::TtsAPI::GetFactoryInstance()->Get(
      cast_browser_process_->browser_context());
#endif

  // Initializing metrics service and network delegates must happen after cast
  // service is intialized because CastMetricsServiceClient and
  // CastNetworkDelegate may use components initialized by cast service.
  cast_browser_process_->metrics_service_client()->Initialize();
  url_request_context_factory_->InitializeNetworkDelegates();

  cast_browser_process_->cast_service()->Start();
}

#if defined(OS_ANDROID)
void CastBrowserMainParts::StartPeriodicCrashReportUpload() {
  OnStartPeriodicCrashReportUpload();
  crash_reporter_timer_.reset(new base::RepeatingTimer());
  crash_reporter_timer_->Start(
      FROM_HERE, base::TimeDelta::FromMinutes(20), this,
      &CastBrowserMainParts::OnStartPeriodicCrashReportUpload);
}

void CastBrowserMainParts::OnStartPeriodicCrashReportUpload() {
  base::FilePath crash_dir;
  CrashHandler::GetCrashDumpLocation(&crash_dir);
  CrashHandler::UploadDumps(crash_dir, "", "");
}
#endif  // defined(OS_ANDROID)

bool CastBrowserMainParts::MainMessageLoopRun(int* result_code) {
#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
  return true;
#else
  base::RunLoop run_loop;
  base::Closure quit_closure(run_loop.QuitClosure());

#if !defined(OS_FUCHSIA)
  // Fuchsia doesn't have signals.
  RegisterClosureOnSignal(quit_closure);
#endif  // !defined(OS_FUCHSIA)

  // If parameters_.ui_task is not NULL, we are running browser tests.
  if (parameters_.ui_task) {
    base::MessageLoopCurrent message_loop =
        base::MessageLoopCurrentForUI::Get();
    message_loop->task_runner()->PostTask(FROM_HERE, *parameters_.ui_task);
    message_loop->task_runner()->PostTask(FROM_HERE, quit_closure);
  }

  run_loop.Run();

#if !defined(OS_FUCHSIA)
  // Once the main loop has stopped running, we give the browser process a few
  // seconds to stop cast service and finalize all resources. If a hang occurs
  // and cast services refuse to terminate successfully, then we SIGKILL the
  // current process to avoid indefinite hangs.
  //
  // TODO(sergeyu): Fuchsia doesn't implement POSIX signals. Implement a
  // different shutdown watchdog mechanism.
  RegisterKillOnAlarm(kKillOnAlarmTimeoutSec);
#endif  // !defined(OS_FUCHSIA)

  cast_browser_process_->cast_service()->Stop();
  return true;
#endif
}

void CastBrowserMainParts::PostMainMessageLoopRun() {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context());
  extensions::ExtensionsBrowserClient::Set(nullptr);
  extensions_browser_client_.reset();
  user_pref_service_.reset();
  cast_browser_process_->ClearAccessibilityManager();
#endif

#if defined(OS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
#else
  window_manager_.reset();

  cast_browser_process_->cast_service()->Finalize();
  cast_browser_process_->metrics_service_client()->Finalize();
  cast_browser_process_.reset();

#if !defined(OS_FUCHSIA)
  DeregisterKillOnAlarm();
#endif  // !defined(OS_FUCHSIA)
#endif
}

void CastBrowserMainParts::PostDestroyThreads() {
#if !defined(OS_ANDROID)
  cast_content_browser_client_->ResetMediaResourceTracker();
#endif  // !defined(OS_ANDROID)
}

}  // namespace shell
}  // namespace chromecast
