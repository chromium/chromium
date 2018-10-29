// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_browser_client.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chromecast/base/cast_constants.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/application_session_id_manager.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_main_parts.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_http_user_agent_settings.h"
#include "chromecast/browser/cast_navigation_ui_data.h"
#include "chromecast/browser/cast_network_delegate.h"
#include "chromecast/browser/cast_quota_permission_context.h"
#include "chromecast/browser/cast_resource_dispatcher_host_delegate.h"
#include "chromecast/browser/default_navigation_throttle.h"
#include "chromecast/browser/devtools/cast_devtools_manager_delegate.h"
#include "chromecast/browser/grit/cast_browser_resources.h"
#include "chromecast/browser/media/media_caps_impl.h"
#include "chromecast/browser/service/cast_service_simple.h"
#include "chromecast/browser/tts/tts_controller.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "chromecast/common/global_descriptors.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/base/media_resource_tracker.h"
#include "chromecast/media/cma/backend/cma_backend_factory_impl.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "components/network_hints/browser/network_hints_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "media/audio/audio_thread_impl.h"
#include "media/mojo/buildflags.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/service_manager/embedder/descriptors.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
#include "chromecast/media/service/cast_mojo_media_client.h"
#include "media/mojo/interfaces/constants.mojom.h"  // nogncheck
#include "media/mojo/services/media_service.h"      // nogncheck
#endif  // ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_ANDROID)
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "media/mojo/services/android_mojo_media_client.h"
#if !BUILDFLAG(USE_CHROMECAST_CDMS)
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "url/origin.h"
#endif  // !BUILDFLAG(USE_CHROMECAST_CDMS)
#else
#include "chromecast/browser/memory_pressure_controller_impl.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(USE_CHROMECAST_CDMS)
#include "chromecast/media/cdm/cast_cdm_factory.h"
#endif  // BUILDFLAG(USE_CHROMECAST_CDMS)

#if defined(USE_ALSA)
#include "chromecast/media/audio/cast_audio_manager_alsa.h"  // nogncheck
#endif  // defined(USE_ALSA)

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/browser/cast_extension_message_filter.h"  // nogncheck
#include "extensions/browser/extension_message_filter.h"  // nogncheck
#include "extensions/browser/extension_registry.h"        // nogncheck
#include "extensions/browser/extension_system.h"          // nogncheck
#include "extensions/browser/guest_view/extensions_guest_view_message_filter.h"  // nogncheck
#include "extensions/browser/info_map.h"                            // nogncheck
#include "extensions/browser/io_thread_extension_message_filter.h"  // nogncheck
#include "extensions/browser/process_map.h"                         // nogncheck
#endif

namespace chromecast {
namespace shell {

namespace {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
static std::unique_ptr<service_manager::Service> CreateMediaService(
    CastContentBrowserClient* browser_client) {
#if defined(OS_ANDROID)
  return std::make_unique<::media::MediaService>(
      std::make_unique<::media::AndroidMojoMediaClient>());
#else
  auto mojo_media_client = std::make_unique<media::CastMojoMediaClient>(
      browser_client->GetCmaBackendFactory(),
      base::Bind(&CastContentBrowserClient::CreateCdmFactory,
                 base::Unretained(browser_client)),
      browser_client->GetVideoModeSwitcher(),
      browser_client->GetVideoResolutionPolicy(),
      browser_client->media_resource_tracker());
  return std::make_unique<::media::MediaService>(std::move(mojo_media_client));
#endif  // defined(OS_ANDROID)
}
#endif  // BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)

#if defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)
void CreateMediaDrmStorage(content::RenderFrameHost* render_frame_host,
                           ::media::mojom::MediaDrmStorageRequest request) {
  DVLOG(1) << __func__;
  PrefService* pref_service = CastBrowserProcess::GetInstance()->pref_service();
  DCHECK(pref_service);

  if (render_frame_host->GetLastCommittedOrigin().opaque()) {
    DVLOG(1) << __func__ << ": Unique origin.";
    return;
  }

  // The object will be deleted on connection error, or when the frame navigates
  // away.
  new cdm::MediaDrmStorageImpl(render_frame_host, pref_service,
                               std::move(request));
}
#endif  // defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)

}  // namespace

CastContentBrowserClient::CastContentBrowserClient()
    : cast_browser_main_parts_(nullptr),
      url_request_context_factory_(new URLRequestContextFactory()) {}

CastContentBrowserClient::~CastContentBrowserClient() {
  DCHECK(!media_resource_tracker_)
      << "ResetMediaResourceTracker was not called";
  content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                     url_request_context_factory_.release());
}

std::unique_ptr<CastService> CastContentBrowserClient::CreateCastService(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    net::URLRequestContextGetter* request_context_getter,
    media::VideoPlaneController* video_plane_controller,
    CastWindowManager* window_manager) {
  return std::make_unique<CastServiceSimple>(browser_context, pref_service,
                                             window_manager);
}

media::VideoModeSwitcher* CastContentBrowserClient::GetVideoModeSwitcher() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
CastContentBrowserClient::GetMediaTaskRunner() {
#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
  if (!media_thread_) {
    media_thread_.reset(new base::Thread("CastMediaThread"));
    base::Thread::Options options;
    options.priority = base::ThreadPriority::REALTIME_AUDIO;
    CHECK(media_thread_->StartWithOptions(options));
    // Start the media_resource_tracker as soon as the media thread is created.
    // There are services that run on the media thread that depend on it,
    // and we want to initialize it with the correct task runner before any
    // tasks that might use it are posted to the media thread.
    media_resource_tracker_ = new media::MediaResourceTracker(
        base::ThreadTaskRunnerHandle::Get(), media_thread_->task_runner());
  }
  return media_thread_->task_runner();
#else
  return nullptr;
#endif
}

#if BUILDFLAG(IS_CAST_USING_CMA_BACKEND)
media::VideoResolutionPolicy*
CastContentBrowserClient::GetVideoResolutionPolicy() {
  return nullptr;
}

media::CmaBackendFactory* CastContentBrowserClient::GetCmaBackendFactory() {
  DCHECK(GetMediaTaskRunner()->BelongsToCurrentThread());
  if (!cma_backend_factory_) {
    cma_backend_factory_ = std::make_unique<media::CmaBackendFactoryImpl>(
        media_pipeline_backend_manager());
  }
  return cma_backend_factory_.get();
}

media::MediaResourceTracker*
CastContentBrowserClient::media_resource_tracker() {
  DCHECK(media_thread_);
  return media_resource_tracker_;
}

void CastContentBrowserClient::ResetMediaResourceTracker() {
  media_resource_tracker_->FinalizeAndDestroy();
  media_resource_tracker_ = nullptr;
}

media::MediaPipelineBackendManager*
CastContentBrowserClient::media_pipeline_backend_manager() {
  DCHECK(cast_browser_main_parts_);
  return cast_browser_main_parts_->media_pipeline_backend_manager();
}

std::unique_ptr<::media::AudioManager>
CastContentBrowserClient::CreateAudioManager(
    ::media::AudioLogFactory* audio_log_factory) {
  // TODO(alokp): Consider switching off the mixer on audio platforms
  // because we already have a mixer in the audio pipeline downstream of
  // CastAudioManager.
#if !defined(OS_ANDROID)
  bool use_mixer = true;
#else
  bool use_mixer = false;
#endif

#if defined(USE_ALSA)
  return std::make_unique<media::CastAudioManagerAlsa>(
      std::make_unique<::media::AudioThreadImpl>(), audio_log_factory,
      base::BindRepeating(&CastContentBrowserClient::GetCmaBackendFactory,
                          base::Unretained(this)),
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      GetMediaTaskRunner(),
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      use_mixer);
#else
  return std::make_unique<media::CastAudioManager>(
      std::make_unique<::media::AudioThreadImpl>(), audio_log_factory,
      base::BindRepeating(&CastContentBrowserClient::GetCmaBackendFactory,
                          base::Unretained(this)),
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::UI}),
      GetMediaTaskRunner(),
      content::ServiceManagerConnection::GetForProcess()->GetConnector(),
      use_mixer);
#endif  // defined(USE_ALSA)
}

bool CastContentBrowserClient::OverridesAudioManager() {
  // See CreateAudioManager().
  return true;
}
#endif  // BUILDFLAG(IS_CAST_USING_CMA_BACKEND)

#if BUILDFLAG(USE_CHROMECAST_CDMS)
std::unique_ptr<::media::CdmFactory>
CastContentBrowserClient::CreateCdmFactory() {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  return std::make_unique<media::CastCdmFactory>(GetMediaTaskRunner(),
                                                 media_resource_tracker());
#endif  // BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  return nullptr;
}
#endif  // BUILDFLAG(USE_CHROMECAST_CDMS)

media::MediaCapsImpl* CastContentBrowserClient::media_caps() {
  DCHECK(cast_browser_main_parts_);
  return cast_browser_main_parts_->media_caps();
}

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
base::WeakPtr<device::BluetoothAdapterCast>
CastContentBrowserClient::CreateBluetoothAdapter() {
  NOTREACHED() << "Bluetooth Adapter is not supported!";
  return nullptr;
}
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

void CastContentBrowserClient::SetMetricsClientId(
    const std::string& client_id) {}

void CastContentBrowserClient::RegisterMetricsProviders(
    ::metrics::MetricsService* metrics_service) {}

bool CastContentBrowserClient::EnableRemoteDebuggingImmediately() {
  return true;
}

content::BrowserMainParts* CastContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  DCHECK(!cast_browser_main_parts_);
  auto main_parts = CastBrowserMainParts::Create(
      parameters, url_request_context_factory_.get(), this);
  cast_browser_main_parts_ = main_parts.get();
  CastBrowserProcess::GetInstance()->SetCastContentBrowserClient(this);

  // TODO(halliwell): would like to change CreateBrowserMainParts to return
  // unique_ptr, then we don't have to release.
  return main_parts.release();
}

void CastContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host,
    service_manager::mojom::ServiceRequest* service_request) {
  // Forcibly trigger I/O-thread URLRequestContext initialization before
  // getting HostResolver.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(
          &net::URLRequestContextGetter::GetURLRequestContext,
          base::Unretained(url_request_context_factory_->GetSystemGetter())),
      base::Bind(&CastContentBrowserClient::AddNetworkHintsMessageFilter,
                 base::Unretained(this), host->GetID()));

#if defined(OS_ANDROID)
  // Cast on Android always allows persisting data.
  //
  // Cast on Android build always uses kForceVideoOverlays command line switch
  // such that secure codecs can always be rendered.
  //
  // TODO(yucliu): On Clank, secure codecs support is tied to AndroidOverlay.
  // Remove kForceVideoOverlays and swtich to the Clank model for secure codecs
  // support.
  host->AddFilter(new cdm::CdmMessageFilterAndroid(true, true));
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  int render_process_id = host->GetID();
  content::BrowserContext* browser_context =
      cast_browser_main_parts_->browser_context();
  host->AddFilter(new extensions::ExtensionMessageFilter(render_process_id,
                                                         browser_context));
  host->AddFilter(new extensions::IOThreadExtensionMessageFilter(
      render_process_id, browser_context));
  host->AddFilter(new extensions::ExtensionsGuestViewMessageFilter(
      render_process_id, browser_context));
  host->AddFilter(
      new CastExtensionMessageFilter(render_process_id, browser_context));
#endif
}

void CastContentBrowserClient::AddNetworkHintsMessageFilter(
    int render_process_id,
    net::URLRequestContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host)
    return;

  scoped_refptr<content::BrowserMessageFilter> network_hints_message_filter(
      new network_hints::NetworkHintsMessageFilter(render_process_id));
  host->AddFilter(network_hints_message_filter.get());
}

bool CastContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  static const char* const kProtocolList[] = {
      content::kChromeUIScheme, content::kChromeDevToolsScheme,
      kChromeResourceScheme,    url::kBlobScheme,
      url::kDataScheme,         url::kFileSystemScheme,
  };

  const std::string& scheme = url.scheme();
  for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
    if (scheme == kProtocolList[i])
      return true;
  }

  if (scheme == url::kFileScheme) {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableLocalFileAccesses);
  }

  return false;
}

void CastContentBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  // If this isn't an extension renderer there's nothing to do.
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext());
  const extensions::Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(
          site_instance->GetSiteURL());
  if (!extension)
    return;
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(
          cast_browser_main_parts_->browser_context());

  extensions::ProcessMap::Get(cast_browser_main_parts_->browser_context())
      ->Insert(extension->id(), site_instance->GetProcess()->GetID(),
               site_instance->GetId());

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&extensions::InfoMap::RegisterExtensionProcess,
                     extension_system->info_map(), extension->id(),
                     site_instance->GetProcess()->GetID(),
                     site_instance->GetId()));
#endif
}

void CastContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  std::string process_type =
      command_line->GetSwitchValueNative(switches::kProcessType);
  base::CommandLine* browser_command_line =
      base::CommandLine::ForCurrentProcess();

#if !defined(OS_FUCHSIA)
  // IsCrashReporterEnabled() is set when InitCrashReporter() is called, and
  // controlled by GetBreakpadClient()->EnableBreakpadForProcess(), therefore
  // it's ok to add switch to every process here.
  if (breakpad::IsCrashReporterEnabled()) {
    command_line->AppendSwitch(switches::kEnableCrashReporter);
  }
#endif  // !defined(OS_FUCHSIA)

  // Command-line for different processes.
  if (process_type == switches::kRendererProcess) {
    // Any browser command-line switches that should be propagated to
    // the renderer go here.
    static const char* const kForwardSwitches[] = {
        switches::kForceMediaResolutionHeight,
        switches::kForceMediaResolutionWidth};
    command_line->CopySwitchesFrom(*browser_command_line, kForwardSwitches,
                                   arraysize(kForwardSwitches));
  } else if (process_type == switches::kUtilityProcess) {
    if (browser_command_line->HasSwitch(switches::kAudioOutputChannels)) {
      command_line->AppendSwitchASCII(switches::kAudioOutputChannels,
                                      browser_command_line->GetSwitchValueASCII(
                                          switches::kAudioOutputChannels));
    }
  } else if (process_type == switches::kGpuProcess) {
#if defined(OS_LINUX)
  // Necessary for accelerated 2d canvas.  By default on Linux, Chromium assumes
  // GLES2 contexts can be lost to a power-save mode, which breaks GPU canvas
  // apps.
    command_line->AppendSwitch(switches::kGpuNoContextLost);
#endif

#if defined(USE_AURA)
    static const char* const kForwardSwitches[] = {
        switches::kCastInitialScreenHeight, switches::kCastInitialScreenWidth,
        switches::kVSyncInterval,
    };
    command_line->CopySwitchesFrom(*browser_command_line, kForwardSwitches,
                                   arraysize(kForwardSwitches));

    auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
    gfx::Size res = display.GetSizeInPixel();
    if (display.rotation() == display::Display::ROTATE_90 ||
        display.rotation() == display::Display::ROTATE_270) {
      res = gfx::Size(res.height(), res.width());
    }

    if (!command_line->HasSwitch(switches::kCastInitialScreenWidth)) {
      command_line->AppendSwitchASCII(switches::kCastInitialScreenWidth,
                                      base::IntToString(res.width()));
    }
    if (!command_line->HasSwitch(switches::kCastInitialScreenHeight)) {
      command_line->AppendSwitchASCII(switches::kCastInitialScreenHeight,
                                      base::IntToString(res.height()));
    }

    if (chromecast::IsFeatureEnabled(kSingleBuffer)) {
      command_line->AppendSwitchASCII(switches::kGraphicsBufferCount, "1");
    } else if (chromecast::IsFeatureEnabled(chromecast::kTripleBuffer720)) {
      command_line->AppendSwitchASCII(switches::kGraphicsBufferCount, "3");
    }
#endif  // defined(USE_AURA)
  }
}

std::string CastContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return CastHttpUserAgentSettings::AcceptLanguage();
}

void CastContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  prefs->allow_scripts_to_close_windows = true;
  // TODO(halliwell): http://crbug.com/391089. This pref defaults to to true
  // because some content providers such as YouTube use plain http requests
  // to retrieve media data chunks while running in a https page. This pref
  // should be disabled once all the content providers are no longer doing that.
  prefs->allow_running_insecure_content = true;

  // Enable 5% margins for WebVTT cues to keep within title-safe area
  prefs->text_track_margin_percentage = 5;

  prefs->hide_scrollbars = true;

#if defined(OS_ANDROID)
  // Enable the television style for viewport so that all cast apps have a
  // 1280px wide layout viewport by default.
  DCHECK(prefs->viewport_enabled);
  DCHECK(prefs->viewport_meta_enabled);
  prefs->viewport_style = content::ViewportStyle::TELEVISION;
#endif  // defined(OS_ANDROID)
}

void CastContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new CastResourceDispatcherHostDelegate);
  content::ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

std::string CastContentBrowserClient::GetApplicationLocale() {
  const std::string locale(base::i18n::GetConfiguredLocale());
  return locale.empty() ? "en-US" : locale;
}

content::QuotaPermissionContext*
CastContentBrowserClient::CreateQuotaPermissionContext() {
  return new CastQuotaPermissionContext();
}

void CastContentBrowserClient::GetQuotaSettings(
    content::BrowserContext* context,
    content::StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  storage::GetNominalDynamicSettings(
      partition->GetPath(), context->IsOffTheRecord(), std::move(callback));
}

void CastContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    content::ResourceType resource_type,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  // Allow developers to override certificate errors.
  // Otherwise, any fatal certificate errors will cause an abort.
  if (!callback.is_null()) {
    callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  }
  return;
}

void CastContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  GURL requesting_url("https://" + cert_request_info->host_and_port.ToString());

  if (!requesting_url.is_valid()) {
    LOG(ERROR) << "Invalid URL string: "
               << requesting_url.possibly_invalid_spec();
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  // In our case there are no relevant certs in |client_certs|. The cert
  // we need to return (if permitted) is the Cast device cert, which we can
  // access directly through the ClientAuthSigner instance. However, we need to
  // be on the IO thread to determine whether the app is whitelisted to return
  // it, because CastNetworkDelegate is bound to the IO thread.
  // Subsequently, the callback must then itself be performed back here
  // on the UI thread.
  //
  // TODO(davidben): Stop using child ID to identify an app.
  std::string session_id =
      CastNavigationUIData::GetSessionIdForWebContents(web_contents);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &CastContentBrowserClient::SelectClientCertificateOnIOThread,
          base::Unretained(this), requesting_url, session_id,
          web_contents->GetMainFrame()->GetProcess()->GetID(),
          web_contents->GetMainFrame()->GetRoutingID(),
          base::SequencedTaskRunnerHandle::Get(),
          base::Bind(
              &content::ClientCertificateDelegate::ContinueWithCertificate,
              base::Owned(delegate.release()))));
}

void CastContentBrowserClient::SelectClientCertificateOnIOThread(
    GURL requesting_url,
    const std::string& session_id,
    int render_process_id,
    int render_frame_id,
    scoped_refptr<base::SequencedTaskRunner> original_runner,
    const base::Callback<void(scoped_refptr<net::X509Certificate>,
                              scoped_refptr<net::SSLPrivateKey>)>&
        continue_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CastNetworkDelegate* network_delegate =
      url_request_context_factory_->app_network_delegate();
  if (network_delegate->IsWhitelisted(requesting_url, session_id,
                                      render_process_id, render_frame_id,
                                      false)) {
    original_runner->PostTask(
        FROM_HERE,
        base::BindOnce(continue_callback, DeviceCert(), DeviceKey()));
    return;
  } else {
    LOG(ERROR) << "Invalid host for client certificate request: "
               << requesting_url.host()
               << " with render_process_id: " << render_process_id
               << " and render_frame_id: " << render_frame_id;
  }
  original_runner->PostTask(
      FROM_HERE, base::BindOnce(continue_callback, nullptr, nullptr));
}

bool CastContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  *no_javascript_access = true;
  return false;
}

void CastContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  registry->AddInterface(
      base::Bind(&media::MediaCapsImpl::AddBinding,
                 base::Unretained(cast_browser_main_parts_->media_caps())),
      base::ThreadTaskRunnerHandle::Get());

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  if (!memory_pressure_controller_) {
    memory_pressure_controller_.reset(new MemoryPressureControllerImpl());
  }

  registry->AddInterface(
      base::Bind(&MemoryPressureControllerImpl::AddBinding,
                 base::Unretained(memory_pressure_controller_.get())),
      base::ThreadTaskRunnerHandle::Get());
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
}

void CastContentBrowserClient::ExposeInterfacesToMediaService(
    service_manager::BinderRegistry* registry,
    content::RenderFrameHost* render_frame_host) {
#if defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)
  registry->AddInterface(
      base::BindRepeating(&CreateMediaDrmStorage, render_frame_host));
#endif  // defined(OS_ANDROID) && !BUILDFLAG(USE_CHROMECAST_CDMS)

  std::string application_session_id =
      CastNavigationUIData::GetSessionIdForWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  registry->AddInterface(base::BindRepeating(
      &media::CreateApplicationSessionIdManager, render_frame_host,
      std::move(application_session_id)));
}

void CastContentBrowserClient::RegisterInProcessServices(
    StaticServiceMap* services,
    content::ServiceManagerConnection* connection) {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  service_manager::EmbeddedServiceInfo info;
  info.factory = base::Bind(&CreateMediaService, base::Unretained(this));
  info.task_runner = GetMediaTaskRunner();
  services->insert(std::make_pair(::media::mojom::kMediaServiceName, info));
#endif
}

std::unique_ptr<base::Value>
CastContentBrowserClient::GetServiceManifestOverlay(
    base::StringPiece service_name) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  int id = -1;
  if (service_name == content::mojom::kBrowserServiceName) {
    id = IDR_CAST_CONTENT_BROWSER_MANIFEST_OVERLAY;
  } else if (service_name == content::mojom::kPackagedServicesServiceName) {
    id = IDR_CAST_CONTENT_PACKAGED_SERVICES_MANIFEST_OVERLAY;
  } else if (service_name == content::mojom::kRendererServiceName) {
    id = IDR_CAST_CONTENT_RENDERER_MANIFEST_OVERLAY;
  } else {
    return nullptr;
  }
  base::StringPiece manifest_contents =
      rb.GetRawDataResourceForScale(id, ui::ScaleFactor::SCALE_FACTOR_NONE);
  return base::JSONReader::Read(manifest_contents);
}

void CastContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  mappings->ShareWithRegion(
      kAndroidPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kAndroidPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kAndroidPakDescriptor));
  crash_reporter::ChildExitObserver::GetInstance()->BrowserChildProcessStarted(
      child_process_id, mappings);
#elif !defined(OS_FUCHSIA)
  // TODO(crbug.com/753619): Enable crash reporting on Fuchsia.
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(service_manager::kCrashDumpSignal, crash_signal_fd);
  }
#endif  // !defined(OS_FUCHSIA)
}

void CastContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(kChromeResourceScheme);
}

content::DevToolsManagerDelegate*
CastContentBrowserClient::GetDevToolsManagerDelegate() {
  return new CastDevToolsManagerDelegate();
}

std::unique_ptr<content::NavigationUIData>
CastContentBrowserClient::GetNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  DCHECK(web_contents);

  std::string session_id =
      CastNavigationUIData::GetSessionIdForWebContents(web_contents);
  return std::make_unique<CastNavigationUIData>(session_id);
}

bool CastContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  return false;
}

scoped_refptr<net::X509Certificate> CastContentBrowserClient::DeviceCert() {
  return nullptr;
}

scoped_refptr<net::SSLPrivateKey> CastContentBrowserClient::DeviceKey() {
  return nullptr;
}

#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
int CastContentBrowserClient::GetCrashSignalFD(
    const base::CommandLine& command_line) {
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess ||
      process_type == switches::kGpuProcess ||
      process_type == switches::kUtilityProcess) {
    breakpad::CrashHandlerHostLinux* crash_handler =
        crash_handlers_[process_type];
    if (!crash_handler) {
      crash_handler = CreateCrashHandlerHost(process_type);
      crash_handlers_[process_type] = crash_handler;
    }
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}

breakpad::CrashHandlerHostLinux*
CastContentBrowserClient::CreateCrashHandlerHost(
    const std::string& process_type) {
  // Let cast shell dump to /tmp. Internal minidump generator code can move it
  // to /data/minidumps later, since /data/minidumps is file lock-controlled.
  base::FilePath dumps_path;
  base::PathService::Get(base::DIR_TEMP, &dumps_path);

  // Alway set "upload" to false to use our own uploader.
  breakpad::CrashHandlerHostLinux* crash_handler =
      new breakpad::CrashHandlerHostLinux(process_type, dumps_path,
                                          false /* upload */);
  // StartUploaderThread() even though upload is diferred.
  // Breakpad-related memory is freed in the uploader thread.
  crash_handler->StartUploaderThread();
  return crash_handler;
}
#endif  // !defined(OS_ANDROID)

std::vector<std::unique_ptr<content::NavigationThrottle>>
CastContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  // If this isn't an extension renderer there's nothing to do.
  content::SiteInstance* site_instance = handle->GetStartingSiteInstance();
  if (site_instance) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(site_instance->GetBrowserContext());
    const extensions::Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(
            site_instance->GetSiteURL());
    if (extension) {
      throttles.push_back(std::make_unique<DefaultNavigationThrottle>(
          handle, content::NavigationThrottle::CANCEL_AND_IGNORE));
    }
  }
#endif
  return throttles;
}

}  // namespace shell
}  // namespace chromecast
