// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_main_parts.h"

#include <stddef.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/base/cast_constants.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/metrics/grouped_histogram.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/browser/cast_feature_list_creator.h"
#include "chromecast/browser/cast_feature_update_observer.h"
#include "chromecast/browser/cast_system_memory_pressure_evaluator.h"
#include "chromecast/browser/cast_system_memory_pressure_evaluator_adjuster.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/browser/media/supported_codec_finder.h"
#include "chromecast/browser/metrics/cast_browser_metrics.h"
#include "chromecast/browser/metrics/metrics_helper_impl.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "chromecast/browser/service_connector.h"
#include "chromecast/browser/service_manager_connection.h"
#include "chromecast/browser/service_manager_context.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/common/mojom/constants.mojom.h"
#include "chromecast/external_mojo/broker_service/broker_service.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "chromecast/external_mojo/external_service_support/external_service.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "chromecast/media/base/key_systems_common.h"
#include "chromecast/media/base/video_plane_controller.h"
#include "chromecast/media/common/media_pipeline_backend_manager.h"
#include "chromecast/media/common/media_resource_tracker.h"
#include "chromecast/metrics/cast_metrics_service_client.h"
#include "chromecast/net/connectivity_checker.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/service/cast_service.h"
#include "chromecast/ui/display_settings_manager_impl.h"
#include "components/heap_profiling/multi_process/client_connection_manager.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "components/prefs/pref_service.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <fontconfig/fontconfig.h>
#include <signal.h>
#include <sys/prctl.h>
#include "ui/gfx/linux/fontconfig_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include "chromecast/net/network_change_notifier_factory_fuchsia.h"
#else
#include "chromecast/net/network_change_notifier_factory_cast.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "chromecast/net/fake_connectivity_checker.h"
#endif

#if defined(USE_AURA)
// gn check ignored on OverlayManagerCast as it's not a public ozone
// header, but is exported to allow injecting the overlay-composited
// callback.
#include "chromecast/browser/cast_display_configurator.h"  // nogncheck
#include "chromecast/browser/devtools/cast_ui_devtools.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/graphics/cast_window_manager_aura.h"
#include "chromecast/media/service/cast_renderer.h"  // nogncheck
#if !BUILDFLAG(IS_FUCHSIA)
#include "components/ui_devtools/devtools_server.h"  // nogncheck
#include "components/ui_devtools/switches.h"         // nogncheck
#endif
#include "ui/display/screen.h"
#include "ui/views/views_delegate.h"  // nogncheck
#else
#include "chromecast/graphics/cast_window_manager_default.h"  // nogncheck
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
#include "device/bluetooth/cast/bluetooth_adapter_cast.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

#if !BUILDFLAG(IS_FUCHSIA)
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/public/cast_sys_info.h"
#endif  // !BUILDFLAG(IS_FUCHSIA)

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
int kSignalsToRunClosure[] = {
    SIGTERM,
    SIGINT,
};
// Closure to run on SIGTERM and SIGINT.
base::OnceClosure* g_signal_closure = nullptr;
base::PlatformThreadId g_main_thread_id;

void RunClosureOnSignal(int signum) {
  if (base::PlatformThread::CurrentId() != g_main_thread_id) {
    RAW_LOG(INFO, "Received signal on non-main thread\n");
    return;
  }

  char message[48] = "Received close signal: ";
  strncat(message, strsignal(signum), sizeof(message) - strlen(message) - 1);
  RAW_LOG(INFO, message);

  DCHECK(g_signal_closure);
  if (*g_signal_closure)
    std::move(*g_signal_closure).Run();
}

void RegisterClosureOnSignal(base::OnceClosure closure) {
  DCHECK(!g_signal_closure);
  DCHECK(closure);
  DCHECK_GT(std::size(kSignalsToRunClosure), 0U);

  // Memory leak on purpose, since |g_signal_closure| should live until
  // process exit.
  g_signal_closure = new base::OnceClosure(std::move(closure));
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

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

std::unique_ptr<heap_profiling::ClientConnectionManager>
CreateClientConnectionManager(
    base::WeakPtr<heap_profiling::Controller> controller_weak_ptr,
    heap_profiling::Mode mode) {
  return std::make_unique<heap_profiling::ClientConnectionManager>(
      std::move(controller_weak_ptr), mode);
}

#if defined(USE_AURA)

// Provide a basic implementation. No need to override anything since we're not
// planning on customizing any behavior at this point.
class CastViewsDelegate : public views::ViewsDelegate {
 public:
  CastViewsDelegate() = default;

  CastViewsDelegate(const CastViewsDelegate&) = delete;
  CastViewsDelegate& operator=(const CastViewsDelegate&) = delete;

  ~CastViewsDelegate() override = default;
};

#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

base::FilePath GetApplicationFontsDir() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string fontconfig_sysroot;
  if (env->GetVar("FONTCONFIG_SYSROOT", &fontconfig_sysroot)) {
    // Running with hermetic fontconfig; using the full path will not work.
    // Assume the root is base::DIR_ASSETS as set by
    // test_fonts::SetUpFontconfig().
    return base::FilePath("/fonts");
  } else {
    base::FilePath dir_assets;
    base::PathService::Get(base::DIR_ASSETS, &dir_assets);
    return dir_assets.Append("fonts");
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

namespace chromecast {
namespace shell {

namespace {

struct DefaultCommandLineSwitch {
  const char* const switch_name;
  const char* const switch_value;
};

const DefaultCommandLineSwitch kDefaultSwitches[] = {
#if !BUILDFLAG(IS_ANDROID)
    // GPU shader disk cache disabling is largely to conserve disk space.
    {switches::kDisableGpuShaderDiskCache, ""},
#endif
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
    {switches::kDisableGpu, ""},
    {switches::kDisableSoftwareRasterizer, ""},
    {switches::kDisableGpuCompositing, ""},
#if BUILDFLAG(IS_ANDROID)
    {switches::kDisableFrameRateLimit, ""},
    {switches::kDisableGLDrawingForTests, ""},
    {cc::switches::kDisableThreadedAnimation, ""},
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_CAST_AUDIO_ONLY)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if defined(ARCH_CPU_X86_FAMILY)
    // This is needed for now to enable the x11 Ozone platform to work with
    // current Linux/NVidia OpenGL drivers.
    {switches::kIgnoreGpuBlocklist, ""},
#elif defined(ARCH_CPU_ARM_FAMILY)
#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
    {switches::kEnableHardwareOverlays, "cast"},
#endif
#endif
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // It's better to start GPU process on demand. For example, for TV platforms
    // cast starts in background and can't render until TV switches to cast
    // input.
    {switches::kDisableGpuEarlyInit, ""},
    // Enable navigator.connection API.
    // TODO(derekjchow): Remove this switch when enabled by default.
    {switches::kEnableNetworkInformationDownlinkMax, ""},
    // TODO(halliwell): Revert after fix for b/63101386.
    {switches::kDisallowNonExactResourceReuse, ""},
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
      DVLOG(2) << "Set default switch '" << name << "' = '" << value << "'";
      command_line->AppendSwitchASCII(name, value);
    } else {
      DVLOG(2) << "Skip setting default switch '" << name << "', already set";
    }
  }
}

}  // namespace

CastBrowserMainParts::CastBrowserMainParts(
    CastContentBrowserClient* cast_content_browser_client)
    : cast_browser_process_(new CastBrowserProcess()),
      cast_content_browser_client_(cast_content_browser_client),
      media_caps_(std::make_unique<media::MediaCapsImpl>()),
      metrics_helper_(std::make_unique<metrics::MetricsHelperImpl>()) {
  DCHECK(cast_content_browser_client);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  AddDefaultCommandLineSwitches(command_line);

  service_manager_context_ = std::make_unique<ServiceManagerContext>(
      cast_content_browser_client_, content::GetUIThreadTaskRunner({}));
  ServiceManagerConnection::GetForProcess()->Start();
}

CastBrowserMainParts::~CastBrowserMainParts() {
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
}

media::MediaPipelineBackendManager*
CastBrowserMainParts::media_pipeline_backend_manager() {
  if (!media_pipeline_backend_manager_) {
    media_pipeline_backend_manager_ =
        std::make_unique<media::MediaPipelineBackendManager>(
            cast_content_browser_client_->GetMediaTaskRunner(),
            cast_content_browser_client_->media_resource_tracker());
  }
  return media_pipeline_backend_manager_.get();
}

media::MediaCapsImpl* CastBrowserMainParts::media_caps() {
  return media_caps_.get();
}

metrics::MetricsHelperImpl* CastBrowserMainParts::metrics_helper() {
  return metrics_helper_.get();
}

content::BrowserContext* CastBrowserMainParts::browser_context() {
  return cast_browser_process_->browser_context();
}

external_mojo::BrokerService* CastBrowserMainParts::broker_service() {
  CHECK(broker_service_);
  return broker_service_.get();
}

external_service_support::ExternalConnector* CastBrowserMainParts::connector() {
  CHECK(connector_);
  return connector_.get();
}

external_service_support::ExternalConnector*
CastBrowserMainParts::media_connector() {
  CHECK(media_connector_);
  return media_connector_.get();
}

CastWebService* CastBrowserMainParts::web_service() {
  return web_service_.get();
}

void CastBrowserMainParts::PreCreateMainMessageLoop() {
  // GroupedHistograms needs to be initialized before any threads are created
  // to prevent race conditions between calls to Preregister and those threads
  // attempting to collect metrics.
  // This call must also be before NetworkChangeNotifier, as it generates
  // Net/DNS metrics.
  metrics::PreregisterAllGroupedHistograms();

#if BUILDFLAG(IS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#elif BUILDFLAG(IS_FUCHSIA)
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryFuchsia());
#else
  net::NetworkChangeNotifier::SetFactory(
      new NetworkChangeNotifierFactoryCast());
#endif
}

void CastBrowserMainParts::PostCreateMainMessageLoop() {
  // Ensure CastMetricsHelper initialized on UI thread.
  metrics::CastMetricsHelper::GetInstance();

#if BUILDFLAG(IS_OZONE)
  // Pass the UI task runner to the ozone platform.
  CHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  ui::OzonePlatform::GetInstance()->PostCreateMainMessageLoop(
      base::DoNothing(), base::SingleThreadTaskRunner::GetCurrentDefault());
#endif  // BUILDFLAG(IS_OZONE)
}

void CastBrowserMainParts::ToolkitInitialized() {
#if defined(USE_AURA)
  // Needs to be initialize before any UI is created.
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_ = std::make_unique<CastViewsDelegate>();
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::FilePath dir_font = GetApplicationFontsDir();
  const FcChar8* dir_font_char8 =
      reinterpret_cast<const FcChar8*>(dir_font.value().data());
  if (!FcConfigAppFontAddDir(gfx::GetGlobalFontConfig(), dir_font_char8)) {
    LOG(ERROR) << "Cannot load fonts from " << dir_font_char8;
  }
#endif
}

int CastBrowserMainParts::PreCreateThreads() {
#if BUILDFLAG(IS_ANDROID)
  child_exit_observer_ = std::make_unique<crash_reporter::ChildExitObserver>();
  child_exit_observer_->RegisterClient(
      std::make_unique<crash_reporter::ChildProcessCrashObserver>());
#endif

  service_connector_ = cast_content_browser_client_->CreateServiceConnector();

  cast_browser_process_->SetPrefService(
      cast_content_browser_client_->GetCastFeatureListCreator()
          ->TakePrefService());

#if defined(USE_AURA)
  cast_screen_ = std::make_unique<CastScreen>();
  cast_browser_process_->SetCastScreen(cast_screen_.get());
  DCHECK(!display::Screen::GetScreen());
  display::Screen::SetScreenInstance(cast_screen_.get());
  cast_browser_process_->SetDisplayConfigurator(
      std::make_unique<CastDisplayConfigurator>(cast_screen_.get()));
#endif  // defined(USE_AURA)

  content::ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      kChromeResourceScheme);
  return 0;
}

void CastBrowserMainParts::PostCreateThreads() {
  if (GetSwitchValueBoolean(switches::kInProcessBroker, true)) {
    auto* service_manager_connector =
        ServiceManagerConnection::GetForProcess()->GetConnector();
    broker_service_ = std::make_unique<external_mojo::BrokerService>(
        service_manager_connector);
    connector_ = external_service_support::ExternalConnector::Create(
        broker_service_->CreateConnector());
  } else {
    connector_ = external_service_support::ExternalConnector::Create(
        external_mojo::GetBrokerPath());
  }
  media_connector_ = connector_->Clone();
  browser_service_ =
      std::make_unique<external_service_support::ExternalService>();
  heap_profiling::Supervisor* supervisor =
      heap_profiling::Supervisor::GetInstance();
  supervisor->SetClientConnectionManagerConstructor(
      &CreateClientConnectionManager);
  supervisor->Start(base::NullCallback());
}

int CastBrowserMainParts::PreMainMessageLoopRun() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // static_cast is safe because this is the only implementation of
  // MemoryPressureMonitor.
  auto* monitor =
      static_cast<memory_pressure::MultiSourceMemoryPressureMonitor*>(
          base::MemoryPressureMonitor::Get());
  // |monitor| may be nullptr in browser tests.
  if (monitor) {
    monitor->SetSystemEvaluator(
        std::make_unique<CastSystemMemoryPressureEvaluator>(
            monitor->CreateVoter()));
  }

  // base::Unretained() is safe because the browser client will outlive any
  // component in the browser; this factory method will not be called after
  // the browser starts to tear down.
  device::BluetoothAdapterCast::SetFactory(base::BindRepeating(
      &CastContentBrowserClient::CreateBluetoothAdapter,
      base::Unretained(cast_browser_process_->browser_client())));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

  cast_content_browser_client_->SetPersistentCookieAccessSettings(
      cast_browser_process_->pref_service());

  cast_browser_process_->SetBrowserContext(
      std::make_unique<CastBrowserContext>());

  cast_browser_process_->SetConnectivityChecker(ConnectivityChecker::Create(
      content::GetIOThreadTaskRunner({}),
      cast_browser_process_->browser_context()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcessIOThread(),
      content::GetNetworkConnectionTracker()));

  cast_browser_process_->SetMetricsServiceClient(
      std::make_unique<metrics::CastMetricsServiceClient>(
          cast_browser_process_->browser_client(),
          cast_browser_process_->pref_service(),
          cast_browser_process_->browser_context()
              ->GetDefaultStoragePartition()
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
  media::CastRenderer::SetOverlayCompositedCallback(BindToCurrentThread(
      base::BindRepeating(&media::VideoPlaneController::SetGeometry,
                          base::Unretained(video_plane_controller_.get()))));
#endif

#if defined(USE_AURA)

#if !BUILDFLAG(IS_FUCHSIA)
  // Start UI devtools if this is a dev device or explicitly enabled.
  // Note that this must happen before the window tree host is created by the
  // window manager.
  auto build_type = CreateSysInfo()->GetBuildType();
  if (CAST_IS_DEBUG_BUILD() || build_type == CastSysInfo::BUILD_ENG ||
      ::ui_devtools::UiDevToolsServer::IsUiDevToolsEnabled(
          ::ui_devtools::switches::kEnableUiDevTools)) {
    // Starts the UI Devtools server for browser Aura UI
    ui_devtools_ =
        std::make_unique<CastUIDevTools>(content::GetIOThreadTaskRunner({}));
  }
#endif

  window_manager_ = std::make_unique<CastWindowManagerAura>(
      CAST_IS_DEBUG_BUILD() ||
      GetSwitchValueBoolean(switches::kEnableInput, false));
  window_manager_->Setup();

  display_change_observer_ = std::make_unique<DisplayConfiguratorObserver>(
      cast_browser_process_->display_configurator(), window_manager_.get());

#else   // defined(USE_AURA)
  window_manager_ = std::make_unique<CastWindowManagerDefault>();
#endif  // defined(USE_AURA)

  cast_content_browser_client_->media_resource_tracker()->InitializeMediaLib();
  ::media::InitializeMediaLibrary();
  // Query the supported codec/profile/levels asynchronously after initializing
  // the media library. This query can block and cause App Not Responding (ANR)
  // errors if CPU resources are tight during browser initialization.
  cast_content_browser_client_->GetMediaTaskRunner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(
              &media::SupportedCodecFinder::FindSupportedCodecProfileLevels),
          base::BindOnce(&CastBrowserMainParts::AddSupportedCodecProfileLevels,
                         weak_factory_.GetWeakPtr()));

  display_settings_manager_ = std::make_unique<DisplaySettingsManagerImpl>(
      window_manager_.get(),
#if defined(USE_AURA)
      cast_browser_process_->display_configurator()
#else
      nullptr
#endif  // defined(USE_AURA)
  );

  web_service_ = std::make_unique<CastWebService>(
      cast_browser_process_->browser_context(), window_manager_.get());
  browser_service_->AddInterface<::chromecast::mojom::CastWebService>(
      web_service_.get());
  connector()->RegisterService(::chromecast::mojom::kCastBrowserServiceName,
                               browser_service_.get());

  cast_browser_process_->SetCastService(
      cast_browser_process_->browser_client()->CreateCastService(
          cast_browser_process_->browser_context(), nullptr,
          cast_browser_process_->pref_service(), video_plane_controller_.get(),
          window_manager_.get(), web_service_.get(),
          display_settings_manager_.get()));
  cast_browser_process_->cast_service()->Initialize();

  // Initializing metrics service and network delegates must happen after cast
  // service is initialized because CastMetricsServiceClient,
  // CastURLLoaderThrottle and CastNetworkDelegate may use components
  // initialized by cast service.
  cast_browser_process_->cast_browser_metrics()->Initialize();
  cast_content_browser_client_->InitializeURLLoaderThrottleDelegate();

  cast_content_browser_client_->CreateGeneralAudienceBrowsingService();

  // Disable RenderFrameHost's Javascript injection restrictions so that the
  // Cast Web Service can implement its own JS injection policy at a higher
  // level.
  content::RenderFrameHost::AllowInjectingJavaScript();

  cast_browser_process_->cast_service()->Start();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseCastBrowserPrefConfig)) {
    feature_update_observer_ = std::make_unique<CastFeatureUpdateObserver>(
        connector(), cast_browser_process_->pref_service());
  }

  return content::RESULT_CODE_NORMAL_EXIT;
}

void CastBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
#if BUILDFLAG(IS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
#elif !BUILDFLAG(IS_FUCHSIA)
  // Fuchsia doesn't have signals.
  RegisterClosureOnSignal(run_loop->QuitClosure());
#endif  // !BUILDFLAG(IS_FUCHSIA)
}

void CastBrowserMainParts::PostMainMessageLoopRun() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // Once the main loop has stopped running, we give the browser process a few
  // seconds to stop cast service and finalize all resources. If a hang occurs
  // and cast services refuse to terminate successfully, then we SIGKILL the
  // current process to avoid indefinite hangs.
  //
  // TODO(sergeyu): Fuchsia doesn't implement POSIX signals. Implement a
  // different shutdown watchdog mechanism.
  RegisterKillOnAlarm(kKillOnAlarmTimeoutSec);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

  cast_browser_process_->cast_service()->Stop();

#if BUILDFLAG(IS_ANDROID)
  // Android does not use native main MessageLoop.
  NOTREACHED();
#else
#if defined(USE_AURA)
  // Reset display change observer here to ensure it is deleted before
  // display_configurator since display_configurator is deleted when
  // `cast_browser_process_` is reset below.
  display_change_observer_.reset();
#endif

  cast_browser_process_->cast_service()->Finalize();
  cast_browser_process_->cast_browser_metrics()->Finalize();
  cast_browser_process_.reset();

  window_manager_.reset();
#if defined(USE_AURA)
  display::Screen::SetScreenInstance(nullptr);
  cast_screen_.reset();
#endif

#if !BUILDFLAG(IS_FUCHSIA)
  DeregisterKillOnAlarm();
#endif  // !BUILDFLAG(IS_FUCHSIA)

  service_manager_context_.reset();
#endif
}

void CastBrowserMainParts::PostDestroyThreads() {
#if !BUILDFLAG(IS_ANDROID)
  cast_content_browser_client_->ResetMediaResourceTracker();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void CastBrowserMainParts::AddSupportedCodecProfileLevels(
    base::span<const media::CodecProfileLevel> codec_profile_levels) {
  LOG(INFO) << "Adding " << codec_profile_levels.size()
            << " supported codec profiles/levels";
  for (const auto& cpl : codec_profile_levels) {
    media_caps_->AddSupportedCodecProfileLevel(cpl);
  }
}

}  // namespace shell
}  // namespace chromecast
