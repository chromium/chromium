// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pdf_util.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/common/plugin.mojom.h"
#include "chrome/common/prerender_types.h"
#include "chrome/common/prerender_url_loader_throttle.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "chrome/common/thread_profiler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/benchmarking_extension.h"
#include "chrome/renderer/browser_exposed_renderer_interfaces.h"
#include "chrome/renderer/chrome_render_frame_observer.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/content_settings_agent_impl.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/media/flash_embed_rewrite.h"
#include "chrome/renderer/media/webrtc_logging_agent_impl.h"
#include "chrome/renderer/net/net_error_helper.h"
#include "chrome/renderer/net_benchmarking_extension.h"
#include "chrome/renderer/pepper/pepper_helper.h"
#include "chrome/renderer/plugins/non_loadable_plugin_placeholder.h"
#include "chrome/renderer/plugins/pdf_plugin_placeholder.h"
#include "chrome/renderer/plugins/plugin_preroller.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "chrome/renderer/prerender/prerender_dispatcher.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/prerender/prerenderer_client.h"
#include "chrome/renderer/previews/resource_loading_hints_agent.h"
#include "chrome/renderer/sync_encryption_keys_extension.h"
#include "chrome/renderer/url_loader_throttle_provider_impl.h"
#include "chrome/renderer/v8_unwinder.h"
#include "chrome/renderer/websocket_handshake_throttle_provider_impl.h"
#include "chrome/renderer/worker_content_settings_client.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/content_capture/common/content_capture_features.h"
#include "components/content_capture/renderer/content_capture_sender.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/contextual_search/content/renderer/overlay_js_render_frame_observer.h"
#include "components/data_reduction_proxy/content/renderer/content_previews_render_frame_observer.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/dom_distiller/content/renderer/distillability_agent.h"
#include "components/dom_distiller/content/renderer/distiller_js_render_frame_observer.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/localized_error.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"
#include "components/pdf/renderer/pepper_pdf_host.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/renderer/threat_dom_details.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/startup_metric_utils/common/startup_metric.mojom.h"
#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "ppapi/buildflags/buildflags.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_visibility_state.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_origin_trials.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/renderer/sandbox_status_extension_android.h"
#else
#include "chrome/renderer/searchbox/search_bouncer.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/renderer/nacl_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/initialize_extensions_client.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/native_theme/native_theme.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/common/plugin_utils.h"
#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"
#include "chrome/renderer/plugins/power_saver_info.h"
#else
#include "components/plugins/renderer/plugin_placeholder.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/renderer/printing/chrome_print_render_frame_helper_delegate.h"
#include "components/printing/renderer/print_render_frame_helper.h"
#include "printing/print_settings.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/renderer/pepper/chrome_pdf_print_client.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "components/spellcheck/renderer/spellcheck_panel.h"
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/renderer/supervised_user/supervised_user_error_page_controller_delegate_impl.h"
#endif

using autofill::AutofillAgent;
using autofill::PasswordAutofillAgent;
using autofill::PasswordGenerationAgent;
using base::ASCIIToUTF16;
using base::UserMetricsAction;
using blink::WebCache;
using blink::WebConsoleMessage;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebPlugin;
using blink::WebPluginParams;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebVector;
using blink::mojom::FetchCacheMode;
using content::PluginInstanceThrottler;
using content::RenderFrame;
using content::RenderThread;
using content::WebPluginInfo;
using content::WebPluginMimeType;
using extensions::Extension;

namespace {

// Whitelist PPAPI for Android Runtime for Chromium. (See crbug.com/383937)
#if BUILDFLAG(ENABLE_PLUGINS)
const char* const kPredefinedAllowedCameraDeviceOrigins[] = {
    "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",
    "4EB74897CB187C7633357C2FE832E0AD6A44883A"};
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
void AppendParams(
    const std::vector<WebPluginMimeType::Param>& additional_params,
    WebVector<WebString>* existing_names,
    WebVector<WebString>* existing_values) {
  DCHECK(existing_names->size() == existing_values->size());
  size_t existing_size = existing_names->size();
  size_t total_size = existing_size + additional_params.size();

  WebVector<WebString> names(total_size);
  WebVector<WebString> values(total_size);

  for (size_t i = 0; i < existing_size; ++i) {
    names[i] = (*existing_names)[i];
    values[i] = (*existing_values)[i];
  }

  for (size_t i = 0; i < additional_params.size(); ++i) {
    names[existing_size + i] = WebString::FromUTF16(additional_params[i].name);
    values[existing_size + i] =
        WebString::FromUTF16(additional_params[i].value);
  }

  existing_names->Swap(names);
  existing_values->Swap(values);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

bool IsStandaloneContentExtensionProcess() {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      extensions::switches::kExtensionProcess);
#endif
}

// Defers media player loading in background pages until they're visible.
class MediaLoadDeferrer : public content::RenderViewObserver {
 public:
  MediaLoadDeferrer(content::RenderView* render_view,
                    base::OnceClosure continue_loading_cb)
      : content::RenderViewObserver(render_view),
        continue_loading_cb_(std::move(continue_loading_cb)) {}
  ~MediaLoadDeferrer() override {}

  // content::RenderFrameObserver implementation:
  void OnDestruct() override { delete this; }
  void OnPageVisibilityChanged(
      content::PageVisibilityState visibility_state) override {
    if (visibility_state != content::PageVisibilityState::kVisible)
      return;
    std::move(continue_loading_cb_).Run();
    delete this;
  }

 private:
  base::OnceClosure continue_loading_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaLoadDeferrer);
};

std::unique_ptr<base::Unwinder> CreateV8Unwinder(
    const v8::UnwindState& unwind_state) {
  return std::make_unique<V8Unwinder>(unwind_state);
}

}  // namespace

ChromeContentRendererClient::ChromeContentRendererClient()
    : main_entry_time_(base::TimeTicks::Now()),
#if defined(OS_WIN)
      remote_module_watcher_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
#endif
      main_thread_profiler_(ThreadProfiler::CreateAndStartOnMainThread()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EnsureExtensionsClientInitialized();
  extensions::ExtensionsRendererClient::Set(
      ChromeExtensionsRendererClient::GetInstance());
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  for (const char* origin : kPredefinedAllowedCameraDeviceOrigins)
    allowed_camera_device_origins_.insert(origin);
#endif
}

ChromeContentRendererClient::~ChromeContentRendererClient() {}

void ChromeContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();

  main_thread_profiler_->SetAuxUnwinderFactory(base::BindRepeating(
      &CreateV8Unwinder, v8::Isolate::GetCurrent()->GetUnwindState()));

  thread->SetRendererProcessType(
      IsStandaloneContentExtensionProcess()
          ? blink::scheduler::WebRendererProcessType::kExtensionRenderer
          : blink::scheduler::WebRendererProcessType::kRenderer);

  {
    mojo::Remote<startup_metric_utils::mojom::StartupMetricHost>
        startup_metric_host;
    thread->BindHostReceiver(startup_metric_host.BindNewPipeAndPassReceiver());
    startup_metric_host->RecordRendererMainEntryTime(main_entry_time_);
  }

#if defined(OS_WIN)
  mojo::PendingRemote<mojom::ModuleEventSink> module_event_sink;
  thread->BindHostReceiver(module_event_sink.InitWithNewPipeAndPassReceiver());
  remote_module_watcher_ = RemoteModuleWatcher::Create(
      thread->GetIOTaskRunner(), std::move(module_event_sink));
#endif

  browser_interface_broker_ =
      blink::Platform::Current()->GetBrowserInterfaceBroker();

  chrome_observer_ = std::make_unique<ChromeRenderThreadObserver>();
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()->RenderThreadStarted();
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
  if (!spellcheck_)
    InitSpellCheck();
#endif

  prerender_dispatcher_.reset(new prerender::PrerenderDispatcher());
  subresource_filter_ruleset_dealer_.reset(
      new subresource_filter::UnverifiedRulesetDealer());

  thread->AddObserver(chrome_observer_.get());
  thread->AddObserver(prerender_dispatcher_.get());
  thread->AddObserver(subresource_filter_ruleset_dealer_.get());

#if !defined(OS_ANDROID)
  thread->AddObserver(SearchBouncer::GetInstance());
#endif

  thread->RegisterExtension(extensions_v8::LoadTimesExtension::Get());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(variations::switches::kEnableBenchmarking))
    thread->RegisterExtension(extensions_v8::BenchmarkingExtension::Get());
  if (command_line->HasSwitch(switches::kEnableNetBenchmarking))
    thread->RegisterExtension(extensions_v8::NetBenchmarkingExtension::Get());

  // chrome-search: and chrome-distiller: pages  should not be accessible by
  // normal content, and should also be unable to script anything but themselves
  // (to help limit the damage that a corrupt page could cause).
  WebString chrome_search_scheme(
      WebString::FromASCII(chrome::kChromeSearchScheme));

  // The Instant process can only display the content but not read it.  Other
  // processes can't display it or read it.
  if (!command_line->HasSwitch(switches::kInstantProcess))
    WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(chrome_search_scheme);

  WebString dom_distiller_scheme(
      WebString::FromASCII(dom_distiller::kDomDistillerScheme));
  // TODO(nyquist): Add test to ensure this happens when the flag is set.
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(dom_distiller_scheme);

#if defined(OS_ANDROID)
  WebSecurityPolicy::RegisterURLSchemeAsAllowedForReferrer(
      WebString::FromUTF8(chrome::kAndroidAppScheme));
#endif

  // chrome-search: pages should not be accessible by bookmarklets
  // or javascript: URLs typed in the omnibox.
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_search_scheme);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  pdf_print_client_.reset(new ChromePDFPrintClient());
  pdf::PepperPDFHost::SetPrintClient(pdf_print_client_.get());
#endif

  for (auto& origin_or_hostname_pattern :
       network::SecureOriginAllowlist::GetInstance().GetCurrentAllowlist()) {
    WebSecurityPolicy::AddOriginToTrustworthySafelist(
        WebString::FromUTF8(origin_or_hostname_pattern));
  }

  for (auto& scheme :
       secure_origin_whitelist::GetSchemesBypassingSecureContextCheck()) {
    WebSecurityPolicy::AddSchemeToSecureContextSafelist(
        WebString::FromASCII(scheme));
  }

  // This doesn't work in single-process mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    ThreadProfiler::SetMainThreadTaskRunner(
        base::ThreadTaskRunnerHandle::Get());
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector;
    thread->BindHostReceiver(collector.InitWithNewPipeAndPassReceiver());
    ThreadProfiler::SetCollectorForChildProcess(std::move(collector));
  }
}

void ChromeContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  // NOTE: Do not add binders directly within this method. Instead, modify the
  // definition of |ExposeChromeRendererInterfacesToBrowser()| to ensure
  // security review coverage.
  ExposeChromeRendererInterfacesToBrowser(this, binders);
}

void ChromeContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  ChromeRenderFrameObserver* render_frame_observer =
      new ChromeRenderFrameObserver(render_frame, web_cache_impl_.get());
  service_manager::BinderRegistry* registry = render_frame_observer->registry();

  bool should_whitelist_for_content_settings =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInstantProcess);
  ContentSettingsAgentImpl* content_settings = new ContentSettingsAgentImpl(
      render_frame, should_whitelist_for_content_settings, registry);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  content_settings->SetExtensionDispatcher(
      ChromeExtensionsRendererClient::GetInstance()->extension_dispatcher());
#endif
  if (chrome_observer_.get()) {
    content_settings->SetContentSettingRules(
        chrome_observer_->content_setting_rules());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()->RenderFrameCreated(
      render_frame, registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  new PepperHelper(render_frame);
#endif

#if BUILDFLAG(ENABLE_NACL)
  new nacl::NaClHelper(render_frame);
#endif

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL) || BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  safe_browsing::ThreatDOMDetails::Create(render_frame, registry);
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<ChromePrintRenderFrameHelperDelegate>());
#endif

#if defined(OS_ANDROID)
  SandboxStatusExtension::Create(render_frame);
#endif

#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kSyncEncryptionKeysWebApi)) {
    SyncEncryptionKeysExtension::Create(render_frame);
  }
#endif

  new NetErrorHelper(render_frame);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  new SupervisedUserErrorPageControllerDelegateImpl(render_frame);
#endif

  if (!render_frame->IsMainFrame()) {
    auto* prerender_helper = prerender::PrerenderHelper::Get(
        render_frame->GetRenderView()->GetMainRenderFrame());
    if (prerender_helper) {
      // Avoid any race conditions from having the browser tell subframes that
      // they're prerendering.
      new prerender::PrerenderHelper(render_frame,
                                     prerender_helper->prerender_mode(),
                                     prerender_helper->histogram_prefix());
    }
  }

  // Set up a mojo service to test if this page is a distiller page.
  new dom_distiller::DistillerJsRenderFrameObserver(
      render_frame, ISOLATED_WORLD_ID_CHROME_INTERNAL, registry);

  if (dom_distiller::ShouldStartDistillabilityService()) {
    // Create DistillabilityAgent to send distillability updates to
    // DistillabilityDriver in the browser process.
    new dom_distiller::DistillabilityAgent(render_frame, DCHECK_IS_ON());
  }

  // Set up a mojo service to test if this page is a contextual search page.
  new contextual_search::OverlayJsRenderFrameObserver(render_frame, registry);

  blink::AssociatedInterfaceRegistry* associated_interfaces =
      render_frame_observer->associated_interfaces();
  PasswordAutofillAgent* password_autofill_agent =
      new PasswordAutofillAgent(render_frame, associated_interfaces);
  PasswordGenerationAgent* password_generation_agent =
      new PasswordGenerationAgent(render_frame, password_autofill_agent,
                                  associated_interfaces);
  new AutofillAgent(render_frame, password_autofill_agent,
                    password_generation_agent, associated_interfaces);

  if (content_capture::features::IsContentCaptureEnabled()) {
    new content_capture::ContentCaptureSender(render_frame,
                                              associated_interfaces);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (content::MimeHandlerViewMode::UsesCrossProcessFrame()) {
    associated_interfaces->AddInterface(base::BindRepeating(
        &extensions::MimeHandlerViewContainerManager::BindReceiver,
        render_frame->GetRoutingID()));
  }
#endif

  // Owned by |render_frame|.
  page_load_metrics::MetricsRenderFrameObserver* metrics_render_frame_observer =
      new page_load_metrics::MetricsRenderFrameObserver(render_frame);
  // There is no render thread, thus no UnverifiedRulesetDealer in
  // ChromeRenderViewTests.
  if (subresource_filter_ruleset_dealer_) {
    // Create AdResourceTracker to tracker ad resource loads at the chrome
    // layer.
    auto ad_resource_tracker =
        std::make_unique<subresource_filter::AdResourceTracker>();
    metrics_render_frame_observer->SetAdResourceTracker(
        ad_resource_tracker.get());
    new subresource_filter::SubresourceFilterAgent(
        render_frame, subresource_filter_ruleset_dealer_.get(),
        std::move(ad_resource_tracker));
  }
  if (render_frame->IsMainFrame()) {
    new previews::ResourceLoadingHintsAgent(
        render_frame_observer->associated_interfaces(), render_frame);
  }

#if !defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstantProcess) &&
      render_frame->IsMainFrame()) {
    new SearchBox(render_frame);
  }
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  new SpellCheckProvider(render_frame, spellcheck_.get(), this);

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
  new SpellCheckPanel(render_frame, registry, this);
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
#endif

  if (render_frame->IsMainFrame())
    new data_reduction_proxy::ContentPreviewsRenderFrameObserver(render_frame);
}

void ChromeContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new prerender::PrerendererClient(render_view);
}

SkBitmap* ChromeContentRendererClient::GetSadPluginBitmap() {
  return const_cast<SkBitmap*>(ui::ResourceBundle::GetSharedInstance()
                                   .GetImageNamed(IDR_SAD_PLUGIN)
                                   .ToSkBitmap());
}

SkBitmap* ChromeContentRendererClient::GetSadWebViewBitmap() {
  return const_cast<SkBitmap*>(ui::ResourceBundle::GetSharedInstance()
                                   .GetImageNamed(IDR_SAD_WEBVIEW)
                                   .ToSkBitmap());
}

bool ChromeContentRendererClient::IsPluginHandledExternally(
    content::RenderFrame* render_frame,
    const blink::WebElement& plugin_element,
    const GURL& original_url,
    const std::string& mime_type) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(plugin_element.HasHTMLTagName("object") ||
         plugin_element.HasHTMLTagName("embed"));
  if (!content::MimeHandlerViewMode::UsesCrossProcessFrame())
    return false;
  // Blink will next try to load a WebPlugin which would end up in
  // OverrideCreatePlugin, sending another IPC only to find out the plugin is
  // not supported. Here it suffices to return false but there should perhaps be
  // a more unified approach to avoid sending the IPC twice.
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  GetPluginInfoHost()->GetPluginInfo(
      render_frame->GetRoutingID(), original_url,
      render_frame->GetWebFrame()->Top()->GetSecurityOrigin(), mime_type,
      &plugin_info);
  // TODO(ekaramad): Not continuing here due to a disallowed status should take
  // us to CreatePlugin. See if more in depths investigation of |status| is
  // necessary here (see https://crbug.com/965747). For now, returning false
  // should take us to CreatePlugin after HTMLPlugInElement which is called
  // through HTMLPlugInElement::LoadPlugin code path.
  if (plugin_info->status != chrome::mojom::PluginStatus::kAllowed &&
      plugin_info->status !=
          chrome::mojom::PluginStatus::kPlayImportantContent) {
    // We could get here when a MimeHandlerView is loaded inside a <webview>
    // which is using permissions API (see WebViewPluginTests).
    ChromeExtensionsRendererClient::DidBlockMimeHandlerViewForDisallowedPlugin(
        plugin_element);
    return false;
  }
  return ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
      plugin_element, original_url, plugin_info->actual_mime_type,
      plugin_info->plugin);
#else
  return false;
#endif
}

v8::Local<v8::Object> ChromeContentRendererClient::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeExtensionsRendererClient::GetInstance()->GetScriptableObject(
      plugin_element, isolate);
#else
  return v8::Local<v8::Object>();
#endif
}

bool ChromeContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const WebPluginParams& params,
    WebPlugin** plugin) {
  std::string orig_mime_type = params.mime_type.Utf8();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!ChromeExtensionsRendererClient::GetInstance()->OverrideCreatePlugin(
          render_frame, params)) {
    return false;
  }
#endif

  GURL url(params.url);
#if BUILDFLAG(ENABLE_PLUGINS)
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  GetPluginInfoHost()->GetPluginInfo(
      render_frame->GetRoutingID(), url,
      render_frame->GetWebFrame()->Top()->GetSecurityOrigin(), orig_mime_type,
      &plugin_info);
  *plugin = CreatePlugin(render_frame, params, *plugin_info);
#else  // !BUILDFLAG(ENABLE_PLUGINS)
  PluginUMAReporter::GetInstance()->ReportPluginMissing(orig_mime_type, url);
  if (orig_mime_type == kPDFMimeType) {
    ReportPDFLoadStatus(
        PDFLoadStatus::kShowedDisabledPluginPlaceholderForEmbeddedPdf);
    if (base::FeatureList::IsEnabled(features::kClickToOpenPDFPlaceholder)) {
      PDFPluginPlaceholder* placeholder =
          PDFPluginPlaceholder::CreatePDFPlaceholder(render_frame, params);
      *plugin = placeholder->plugin();
      return true;
    }
  }
  auto* placeholder = NonLoadablePluginPlaceholder::CreateNotSupportedPlugin(
      render_frame, params);
  *plugin = placeholder->plugin();

#endif  // BUILDFLAG(ENABLE_PLUGINS)
  return true;
}

WebPlugin* ChromeContentRendererClient::CreatePluginReplacement(
    content::RenderFrame* render_frame,
    const base::FilePath& plugin_path) {
  auto* placeholder = NonLoadablePluginPlaceholder::CreateErrorPlugin(
      render_frame, plugin_path);
  return placeholder->plugin();
}

bool ChromeContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool has_played_media_before,
    base::OnceClosure closure) {
  // Don't allow autoplay/autoload of media resources in a page that is hidden
  // and has never played any media before.  We want to allow future loads even
  // when hidden to allow playlist-like functionality.
  //
  // NOTE: This is also used to defer media loading for prerender.
  if ((render_frame->GetRenderView()->GetWebView()->GetVisibilityState() !=
           blink::PageVisibilityState::kVisible &&
       !has_played_media_before) ||
      prerender::PrerenderHelper::IsPrerendering(render_frame)) {
    new MediaLoadDeferrer(render_frame->GetRenderView(), std::move(closure));
    return true;
  }

  std::move(closure).Run();
  return false;
}

#if BUILDFLAG(ENABLE_PLUGINS)

mojo::AssociatedRemote<chrome::mojom::PluginInfoHost>&
ChromeContentRendererClient::GetPluginInfoHost() {
  struct PluginInfoHostHolder {
    PluginInfoHostHolder() {
      RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
          &plugin_info_host);
    }
    ~PluginInfoHostHolder() {}
    mojo::AssociatedRemote<chrome::mojom::PluginInfoHost> plugin_info_host;
  };
  static base::NoDestructor<PluginInfoHostHolder> holder;
  return holder->plugin_info_host;
}

// static
WebPlugin* ChromeContentRendererClient::CreatePlugin(
    content::RenderFrame* render_frame,
    const WebPluginParams& original_params,
    const chrome::mojom::PluginInfo& plugin_info) {
  const WebPluginInfo& info = plugin_info.plugin;
  const std::string& actual_mime_type = plugin_info.actual_mime_type;
  const base::string16& group_name = plugin_info.group_name;
  const std::string& identifier = plugin_info.group_identifier;
  chrome::mojom::PluginStatus status = plugin_info.status;
  GURL url(original_params.url);
  std::string orig_mime_type = original_params.mime_type.Utf8();
  ChromePluginPlaceholder* placeholder = nullptr;

  // If the browser plugin is to be enabled, this should be handled by the
  // renderer, so the code won't reach here due to the early exit in
  // OverrideCreatePlugin.
  if (status == chrome::mojom::PluginStatus::kNotFound ||
      orig_mime_type == content::kBrowserPluginMimeType) {
    PluginUMAReporter::GetInstance()->ReportPluginMissing(orig_mime_type, url);
    placeholder = ChromePluginPlaceholder::CreateLoadableMissingPlugin(
        render_frame, original_params);
  } else {
    // TODO(bauerb): This should be in content/.
    WebPluginParams params(original_params);
    for (const auto& mime_type : info.mime_types) {
      if (mime_type.mime_type == actual_mime_type) {
        AppendParams(mime_type.additional_params, &params.attribute_names,
                     &params.attribute_values);
        break;
      }
    }
    if (params.mime_type.IsNull() && (actual_mime_type.size() > 0)) {
      // Webkit might say that mime type is null while we already know the
      // actual mime type via ChromeViewHostMsg_GetPluginInfo. In that case
      // we should use what we know since WebpluginDelegateProxy does some
      // specific initializations based on this information.
      params.mime_type = WebString::FromUTF8(actual_mime_type);
    }

    ContentSettingsAgentImpl* content_settings_agent =
        ContentSettingsAgentImpl::Get(render_frame);

    const ContentSettingsType content_type =
        ShouldUseJavaScriptSettingForPlugin(info)
            ? ContentSettingsType::JAVASCRIPT
            : ContentSettingsType::PLUGINS;

    if ((status == chrome::mojom::PluginStatus::kUnauthorized ||
         status == chrome::mojom::PluginStatus::kBlocked) &&
        content_settings_agent->IsPluginTemporarilyAllowed(identifier)) {
      status = chrome::mojom::PluginStatus::kAllowed;
    }

    auto create_blocked_plugin = [&render_frame, &params, &info, &identifier,
                                  &group_name](int template_id,
                                               const base::string16& message) {
      return ChromePluginPlaceholder::CreateBlockedPlugin(
          render_frame, params, info, identifier, group_name, template_id,
          message, PowerSaverInfo());
    };
    WebLocalFrame* frame = render_frame->GetWebFrame();
    switch (status) {
      case chrome::mojom::PluginStatus::kNotFound: {
        NOTREACHED();
        break;
      }
      case chrome::mojom::PluginStatus::kAllowed:
      case chrome::mojom::PluginStatus::kPlayImportantContent: {
#if BUILDFLAG(ENABLE_NACL) && BUILDFLAG(ENABLE_EXTENSIONS)
        const bool is_nacl_plugin =
            info.name == ASCIIToUTF16(nacl::kNaClPluginName);
        const bool is_nacl_mime_type =
            actual_mime_type == nacl::kNaClPluginMimeType;
        const bool is_pnacl_mime_type =
            actual_mime_type == nacl::kPnaclPluginMimeType;
        if (is_nacl_plugin || is_nacl_mime_type || is_pnacl_mime_type) {
          bool has_enable_nacl_switch =
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kEnableNaCl);
          bool is_nacl_unrestricted =
              has_enable_nacl_switch || is_pnacl_mime_type;
          GURL manifest_url;
          GURL app_url;
          if (is_nacl_mime_type || is_pnacl_mime_type) {
            // Normal NaCl/PNaCl embed. The app URL is the page URL.
            manifest_url = url;
            app_url = frame->GetDocument().Url();
          } else {
            // NaCl is being invoked as a content handler. Look up the NaCl
            // module using the MIME type. The app URL is the manifest URL.
            manifest_url = GetNaClContentHandlerURL(actual_mime_type, info);
            app_url = manifest_url;
          }
          bool is_module_allowed = false;
          const Extension* extension =
              extensions::RendererExtensionRegistry::Get()
                  ->GetExtensionOrAppByURL(manifest_url);
          if (extension) {
            is_module_allowed =
                IsNativeNaClAllowed(app_url, is_nacl_unrestricted, extension);
          } else {
            WebDocument document = frame->GetDocument();
            is_module_allowed =
                has_enable_nacl_switch ||
                (is_pnacl_mime_type &&
                 blink::WebOriginTrials::isTrialEnabled(&document, "PNaCl"));
          }
          if (!is_module_allowed) {
            WebString error_message;
            if (is_nacl_mime_type) {
              error_message =
                  "Only unpacked extensions and apps installed from the Chrome "
                  "Web Store can load NaCl modules without enabling Native "
                  "Client in about:flags.";
            } else if (is_pnacl_mime_type) {
              error_message =
                  "PNaCl modules can only be used on the open web (non-app/"
                  "extension) when the PNaCl Origin Trial is enabled";
            }
            frame->AddMessageToConsole(WebConsoleMessage(
                blink::mojom::ConsoleMessageLevel::kError, error_message));
            placeholder = create_blocked_plugin(
                IDR_BLOCKED_PLUGIN_HTML,
#if defined(OS_CHROMEOS)
                l10n_util::GetStringUTF16(IDS_NACL_PLUGIN_BLOCKED));
#else
                l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
#endif
            break;
          }
        }
#endif  // BUILDFLAG(ENABLE_NACL) && BUILDFLAG(ENABLE_EXTENSIONS)

        if (GURL(frame->GetDocument().Url()).host_piece() ==
            extension_misc::kPdfExtensionId) {
          if (!base::FeatureList::IsEnabled(features::kWebUIDarkMode)) {
            ui::NativeTheme::GetInstanceForWeb()->set_preferred_color_scheme(
                ui::NativeTheme::PreferredColorScheme::kLight);
          }
        } else if (info.name ==
                   ASCIIToUTF16(ChromeContentClient::kPDFExtensionPluginName)) {
          // Report PDF load metrics. Since the PDF plugin is comprised of an
          // extension that loads a second plugin, avoid double counting by
          // ignoring the creation of the second plugin.
          bool is_main_frame_plugin_document =
              render_frame->IsMainFrame() &&
              render_frame->GetWebFrame()->GetDocument().IsPluginDocument();
          ReportPDFLoadStatus(
              is_main_frame_plugin_document
                  ? PDFLoadStatus::kLoadedFullPagePdfWithPdfium
                  : PDFLoadStatus::kLoadedEmbeddedPdfWithPdfium);
        }

        // Delay loading plugins if prerendering.
        // TODO(mmenke):  In the case of prerendering, feed into
        //                ChromeContentRendererClient::CreatePlugin instead, to
        //                reduce the chance of future regressions.
        bool is_prerendering =
            prerender::PrerenderHelper::IsPrerendering(render_frame);

        bool power_saver_setting_on =
            status == chrome::mojom::PluginStatus::kPlayImportantContent;
        PowerSaverInfo power_saver_info =
            PowerSaverInfo::Get(render_frame, power_saver_setting_on, params,
                                info, frame->GetDocument().Url());
        if (power_saver_info.blocked_for_background_tab || is_prerendering ||
            !power_saver_info.poster_attribute.empty() ||
            power_saver_info.power_saver_enabled) {
          placeholder = ChromePluginPlaceholder::CreateBlockedPlugin(
              render_frame, params, info, identifier, group_name,
              power_saver_info.poster_attribute.empty()
                  ? IDR_BLOCKED_PLUGIN_HTML
                  : IDR_PLUGIN_POSTER_HTML,
              l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name),
              power_saver_info);
          placeholder->set_blocked_for_prerendering(is_prerendering);

          // Because we can't determine the size of a plugin until it loads,
          // all plugins are treated as tiny until proven otherwise.
          placeholder->set_blocked_for_tinyness(
              power_saver_info.power_saver_enabled);

          placeholder->AllowLoading();
          break;
        }

        // Skip the placeholder for non-Flash plugins or if Plugin Power Saver
        // is disabled for testing.
        return render_frame->CreatePlugin(info, params, nullptr);
      }
      case chrome::mojom::PluginStatus::kDisabled: {
        PluginUMAReporter::GetInstance()->ReportPluginDisabled(orig_mime_type,
                                                               url);
        if (info.name ==
            ASCIIToUTF16(ChromeContentClient::kPDFExtensionPluginName)) {
          ReportPDFLoadStatus(
              PDFLoadStatus::kShowedDisabledPluginPlaceholderForEmbeddedPdf);

          if (base::FeatureList::IsEnabled(
                  features::kClickToOpenPDFPlaceholder)) {
            return PDFPluginPlaceholder::CreatePDFPlaceholder(render_frame,
                                                              params)
                ->plugin();
          }
        }

        placeholder = create_blocked_plugin(
            IDR_DISABLED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_DISABLED, group_name));
        break;
      }
      case chrome::mojom::PluginStatus::kFlashHiddenPreferHtml: {
        placeholder = create_blocked_plugin(
            IDR_PREFER_HTML_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_PREFER_HTML_BY_DEFAULT,
                                       group_name));
        break;
      }
      case chrome::mojom::PluginStatus::kOutdatedBlocked: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        placeholder->AllowLoading();
        mojo::AssociatedRemote<chrome::mojom::PluginHost> plugin_host;
        render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
            plugin_host.BindNewEndpointAndPassReceiver());
        plugin_host->BlockedOutdatedPlugin(placeholder->BindPluginRenderer(),
                                           identifier);
        break;
      }
      case chrome::mojom::PluginStatus::kOutdatedDisallowed: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        break;
      }
      case chrome::mojom::PluginStatus::kUnauthorized: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, group_name));
        placeholder->AllowLoading();
        mojo::AssociatedRemote<chrome::mojom::PluginAuthHost> plugin_auth_host;
        render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
            plugin_auth_host.BindNewEndpointAndPassReceiver());
        plugin_auth_host->BlockedUnauthorizedPlugin(group_name, identifier);
        content_settings_agent->DidBlockContentType(content_type);
        break;
      }
      case chrome::mojom::PluginStatus::kBlocked: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
        placeholder->AllowLoading();
        RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Blocked"));
        content_settings_agent->DidBlockContentType(content_type);
        break;
      }
      case chrome::mojom::PluginStatus::kBlockedByPolicy: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED_BY_POLICY,
                                       group_name));
        RenderThread::Get()->RecordAction(
            UserMetricsAction("Plugin_BlockedByPolicy"));
        content_settings_agent->DidBlockContentType(content_type);
        break;
      }
      case chrome::mojom::PluginStatus::kBlockedNoLoading: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED_NO_LOADING,
                                       group_name));
        content_settings_agent->DidBlockContentType(content_type);
        break;
      }
      case chrome::mojom::PluginStatus::kComponentUpdateRequired: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        placeholder->AllowLoading();
        mojo::AssociatedRemote<chrome::mojom::PluginHost> plugin_host;
        render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
            plugin_host.BindNewEndpointAndPassReceiver());
        plugin_host->BlockedComponentUpdatedPlugin(
            placeholder->BindPluginRenderer(), identifier);
        break;
      }

      case chrome::mojom::PluginStatus::kRestartRequired: {
#if defined(OS_LINUX)
        placeholder =
            create_blocked_plugin(IDR_BLOCKED_PLUGIN_HTML,
                                  l10n_util::GetStringFUTF16(
                                      IDS_PLUGIN_RESTART_REQUIRED, group_name));
#endif
        break;
      }
    }
  }
  placeholder->SetStatus(status);
  return placeholder->plugin();
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// For NaCl content handling plugins, the NaCl manifest is stored in an
// additonal 'nacl' param associated with the MIME type.
//  static
GURL ChromeContentRendererClient::GetNaClContentHandlerURL(
    const std::string& actual_mime_type,
    const content::WebPluginInfo& plugin) {
  // Look for the manifest URL among the MIME type's additonal parameters.
  const char kNaClPluginManifestAttribute[] = "nacl";
  base::string16 nacl_attr = ASCIIToUTF16(kNaClPluginManifestAttribute);
  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    if (plugin.mime_types[i].mime_type == actual_mime_type) {
      for (const auto& p : plugin.mime_types[i].additional_params) {
        if (p.name == nacl_attr)
          return GURL(p.value);
      }
      break;
    }
  }
  return GURL();
}

void ChromeContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // TODO(crbug.com/977637): Get rid of the use of this implementation of
  // |service_manager::LocalInterfaceProvider|. This was done only to avoid
  // churning spellcheck code while eliminting the "chrome" and
  // "chrome_renderer" services. Spellcheck is (and should remain) the only
  // consumer of this implementation.
  RenderThread::Get()->BindHostReceiver(
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
}

#if BUILDFLAG(ENABLE_NACL)
//  static
bool ChromeContentRendererClient::IsNativeNaClAllowed(
    const GURL& app_url,
    bool is_nacl_unrestricted,
    const Extension* extension) {
  bool is_invoked_by_webstore_installed_extension = false;
  bool is_extension_unrestricted = false;
  bool is_extension_force_installed = false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool is_extension_from_webstore = extension && extension->from_webstore();

  bool is_invoked_by_extension = app_url.SchemeIs(extensions::kExtensionScheme);
  bool is_invoked_by_hosted_app = extension && extension->is_hosted_app() &&
                                  extension->web_extent().MatchesURL(app_url);

  is_invoked_by_webstore_installed_extension =
      is_extension_from_webstore &&
      (is_invoked_by_extension || is_invoked_by_hosted_app);

  // Allow built-in extensions and developer mode extensions.
  is_extension_unrestricted =
      extension &&
      (extensions::Manifest::IsUnpackedLocation(extension->location()) ||
       extensions::Manifest::IsComponentLocation(extension->location()));
  // Allow extensions force installed by admin policy.
  is_extension_force_installed =
      extension &&
      extensions::Manifest::IsPolicyLocation(extension->location());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Allow NaCl under any of the following circumstances:
  //  1) An extension is loaded unpacked or built-in (component) to Chrome.
  //  2) An extension is force installed by policy.
  //  3) An extension is installed from the webstore, and invoked in that
  //     context (hosted app URL or chrome-extension:// scheme).
  //  4) --enable-nacl is set.
  bool is_nacl_allowed_by_location = is_extension_unrestricted ||
                                     is_extension_force_installed ||
                                     is_invoked_by_webstore_installed_extension;
  bool is_nacl_allowed = is_nacl_allowed_by_location || is_nacl_unrestricted;
  return is_nacl_allowed;
}
#endif  // BUILDFLAG(ENABLE_NACL)

bool ChromeContentRendererClient::HasErrorPage(int http_status_code) {
  // Use an internal error page, if we have one for the status code.
  return error_page::LocalizedError::HasStrings(
      error_page::Error::kHttpErrorDomain, http_status_code);
}

bool ChromeContentRendererClient::ShouldSuppressErrorPage(
    content::RenderFrame* render_frame,
    const GURL& url) {
  // Unit tests for ChromeContentRendererClient pass a NULL RenderFrame here.
  // Unfortunately it's very difficult to construct a mock RenderView, so skip
  // this functionality in this case.
  if (render_frame &&
      NetErrorHelper::Get(render_frame)->ShouldSuppressErrorPage(url)) {
    return true;
  }

  // Do not flash an error page if the Instant new tab page fails to load.
  bool is_instant_ntp = false;
#if !defined(OS_ANDROID)
  is_instant_ntp = SearchBouncer::GetInstance()->IsNewTabPage(url);
#endif
  return is_instant_ntp;
}

bool ChromeContentRendererClient::ShouldTrackUseCounter(const GURL& url) {
  bool is_instant_ntp = false;
#if !defined(OS_ANDROID)
  is_instant_ntp = SearchBouncer::GetInstance()->IsNewTabPage(url);
#endif
  return !is_instant_ntp;
}

void ChromeContentRendererClient::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& web_error,
    const std::string& http_method,
    std::string* error_html) {
  NetErrorHelper::Get(render_frame)
      ->PrepareErrorPage(
          error_page::Error::NetError(web_error.url(), web_error.reason(),
                                      web_error.has_copy_in_cache()),
          http_method == "POST", error_html);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserErrorPageControllerDelegateImpl::Get(render_frame)
      ->PrepareForErrorPage();
#endif
}

void ChromeContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const GURL& unreachable_url,
    const std::string& http_method,
    int http_status,
    std::string* error_html) {
  NetErrorHelper::Get(render_frame)
      ->PrepareErrorPage(
          error_page::Error::HttpError(unreachable_url, http_status),
          http_method == "POST", error_html);
}

void ChromeContentRendererClient::GetErrorDescription(
    const blink::WebURLError& web_error,
    const std::string& http_method,
    base::string16* error_description) {
  error_page::Error error = error_page::Error::NetError(
      web_error.url(), web_error.reason(), web_error.has_copy_in_cache());
  if (error_description) {
    *error_description = error_page::LocalizedError::GetErrorDetails(
        error.domain(), error.reason(), http_method == "POST");
  }
}

void ChromeContentRendererClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner* io_thread_task_runner) {
  io_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ThreadProfiler::StartOnChildThread,
                                metrics::CallStackProfileParams::IO_THREAD));
}

void ChromeContentRendererClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* compositor_thread_task_runner) {
  compositor_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ThreadProfiler::StartOnChildThread,
                     metrics::CallStackProfileParams::COMPOSITOR_THREAD));
}

bool ChromeContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return !IsStandaloneContentExtensionProcess();
}

bool ChromeContentRendererClient::AllowPopup() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeExtensionsRendererClient::GetInstance()->AllowPopup();
#else
  return false;
#endif
}

void ChromeContentRendererClient::WillSendRequest(
    WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    const url::Origin* initiator_origin,
    GURL* new_url,
    bool* attach_same_site_cookies) {
// Check whether the request should be allowed. If not allowed, we reset the
// URL to something invalid to prevent the request and cause an error.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()->WillSendRequest(
      frame, transition_type, url, initiator_origin, new_url,
      attach_same_site_cookies);
  if (!new_url->is_empty())
    return;
#endif

  if (!url.ProtocolIs(chrome::kChromeSearchScheme))
    return;

#if !defined(OS_ANDROID)
  SearchBox* search_box =
      SearchBox::Get(content::RenderFrame::FromWebFrame(frame->LocalRoot()));
  if (search_box) {
    // Note: this GURL copy could be avoided if host() were added to WebURL.
    GURL gurl(url);
    if (gurl.host_piece() == chrome::kChromeUIFaviconHost)
      search_box->GenerateImageURLFromTransientURL(url, new_url);
  }
#endif  // !defined(OS_ANDROID)
}

bool ChromeContentRendererClient::IsPrefetchOnly(
    content::RenderFrame* render_frame,
    const blink::WebURLRequest& request) {
  return prerender::PrerenderHelper::GetPrerenderMode(render_frame) ==
         prerender::PREFETCH_ONLY;
}

uint64_t ChromeContentRendererClient::VisitedLinkHash(const char* canonical_url,
                                                      size_t length) {
  return chrome_observer_->visited_link_slave()->ComputeURLFingerprint(
      canonical_url, length);
}

bool ChromeContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return chrome_observer_->visited_link_slave()->IsVisited(link_hash);
}

blink::WebPrescientNetworking*
ChromeContentRendererClient::GetPrescientNetworking() {
  if (!web_prescient_networking_impl_) {
    web_prescient_networking_impl_ =
        std::make_unique<network_hints::WebPrescientNetworkingImpl>();
  }
  return web_prescient_networking_impl_.get();
}

bool ChromeContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  // TODO(bbudge) remove this when the trusted NaCl plugin has been removed.
  // We must defer certain plugin events for NaCl instances since we switch
  // from the in-process to the out-of-process proxy after instantiating them.
  return module_name == "Native Client";
}

bool ChromeContentRendererClient::IsOriginIsolatedPepperPlugin(
    const base::FilePath& plugin_path) {
  return plugin_path.value() == ChromeContentClient::kPDFPluginPath;
}

#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
bool ChromeContentRendererClient::IsExtensionOrSharedModuleWhitelisted(
    const GURL& url,
    const std::set<std::string>& whitelist) {
  const extensions::ExtensionSet* extension_set =
      extensions::RendererExtensionRegistry::Get()->GetMainThreadExtensionSet();
  return ::IsExtensionOrSharedModuleWhitelisted(url, extension_set, whitelist);
}
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
void ChromeContentRendererClient::InitSpellCheck() {
  spellcheck_ = std::make_unique<SpellCheck>(this);
}
#endif

ChromeRenderThreadObserver* ChromeContentRendererClient::GetChromeObserver()
    const {
  return chrome_observer_.get();
}

web_cache::WebCacheImpl* ChromeContentRendererClient::GetWebCache() {
  return web_cache_impl_.get();
}

chrome::WebRtcLoggingAgentImpl*
ChromeContentRendererClient::GetWebRtcLoggingAgent() {
  if (!webrtc_logging_agent_impl_) {
    webrtc_logging_agent_impl_ =
        std::make_unique<chrome::WebRtcLoggingAgentImpl>();
  }
  return webrtc_logging_agent_impl_.get();
}

#if BUILDFLAG(ENABLE_SPELLCHECK)
SpellCheck* ChromeContentRendererClient::GetSpellCheck() {
  return spellcheck_.get();
}
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
ChromeContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<WebSocketHandshakeThrottleProviderImpl>(
      browser_interface_broker_.get());
}

void ChromeContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
  key_systems_provider_.AddSupportedKeySystems(key_systems);
}

bool ChromeContentRendererClient::IsKeySystemsUpdateNeeded() {
  return key_systems_provider_.IsKeySystemsUpdateNeeded();
}

bool ChromeContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::IsSourceFromAnExtension(source);
#else
  return false;
#endif
}

std::unique_ptr<blink::WebContentSettingsClient>
ChromeContentRendererClient::CreateWorkerContentSettingsClient(
    content::RenderFrame* render_frame) {
  return std::make_unique<WorkerContentSettingsClient>(render_frame);
}

bool ChromeContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
#if BUILDFLAG(ENABLE_PLUGINS)
  // Allow access for tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting)) {
    return true;
  }

  version_info::Channel channel = chrome::GetChannel();
  // Allow dev channel APIs to be used on "Canary", "Dev", and "Unknown"
  // releases of Chrome. Permitting "Unknown" allows these APIs to be used on
  // Chromium builds as well.
  return channel <= version_info::Channel::DEV;
#else
  return false;
#endif
}

bool ChromeContentRendererClient::IsPluginAllowedToUseCameraDeviceAPI(
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting))
    return true;

  if (IsExtensionOrSharedModuleWhitelisted(url, allowed_camera_device_origins_))
    return true;
#endif

  return false;
}

content::BrowserPluginDelegate*
ChromeContentRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const content::WebPluginInfo& info,
    const std::string& mime_type,
    const GURL& original_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeExtensionsRendererClient::CreateBrowserPluginDelegate(
      render_frame, info, mime_type, original_url);
#else
  return nullptr;
#endif
}

void ChromeContentRendererClient::RecordRappor(const std::string& metric,
                                               const std::string& sample) {
  if (!rappor_recorder_)
    RenderThread::Get()->BindHostReceiver(
        rappor_recorder_.BindNewPipeAndPassReceiver());
  rappor_recorder_->RecordRappor(metric, sample);
}

void ChromeContentRendererClient::RecordRapporURL(const std::string& metric,
                                                  const GURL& url) {
  if (!rappor_recorder_)
    RenderThread::Get()->BindHostReceiver(
        rappor_recorder_.BindNewPipeAndPassReceiver());
  rappor_recorder_->RecordRapporURL(metric, url);
}

void ChromeContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()->RunScriptsAtDocumentStart(
      render_frame);
  // |render_frame| might be dead by now.
#endif
}

void ChromeContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()->RunScriptsAtDocumentEnd(
      render_frame);
  // |render_frame| might be dead by now.
#endif
}

void ChromeContentRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()->RunScriptsAtDocumentIdle(
      render_frame);
  // |render_frame| might be dead by now.
#endif
}

void ChromeContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // The performance manager service interfaces are provided by the chrome
  // embedder only.
  blink::WebRuntimeFeatures::EnablePerformanceManagerInstrumentation(true);

// Web Share is shipped on Android, experimental otherwise. It is enabled here,
// in chrome/, to avoid it being made available in other clients of content/
// that do not have a Web Share Mojo implementation.
#if defined(OS_ANDROID)
  blink::WebRuntimeFeatures::EnableWebShare(true);
  blink::WebRuntimeFeatures::EnableWebShareV2(true);
#endif

  if (base::FeatureList::IsEnabled(subresource_filter::kAdTagging))
    blink::WebRuntimeFeatures::EnableAdTagging(true);
}

void ChromeContentRendererClient::
    WillInitializeServiceWorkerContextOnWorkerThread() {
  // This is called on the service worker thread.
  ThreadProfiler::StartOnChildThread(
      metrics::CallStackProfileParams::SERVICE_WORKER_THREAD);
}

void ChromeContentRendererClient::
    DidInitializeServiceWorkerContextOnWorkerThread(
        blink::WebServiceWorkerContextProxy* context_proxy,
        const GURL& service_worker_scope,
        const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()
      ->extension_dispatcher()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context_proxy, service_worker_scope, script_url);
#endif
}

void ChromeContentRendererClient::WillEvaluateServiceWorkerOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()
      ->extension_dispatcher()
      ->WillEvaluateServiceWorkerOnWorkerThread(
          context_proxy, v8_context, service_worker_version_id,
          service_worker_scope, script_url);
#endif
}

void ChromeContentRendererClient::DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()
      ->extension_dispatcher()
      ->DidStartServiceWorkerContextOnWorkerThread(
          service_worker_version_id, service_worker_scope, script_url);
#endif
}

void ChromeContentRendererClient::WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()
      ->extension_dispatcher()
      ->WillDestroyServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id, service_worker_scope, script_url);
#endif
}

bool ChromeContentRendererClient::IsExcludedHeaderForServiceWorkerFetchEvent(
    const std::string& header_name) {
  return variations::IsVariationsHeader(header_name);
}

// If we're in an extension, there is no need disabling multiple routes as
// chrome.system.network.getNetworkInterfaces provides the same
// information. Also, the enforcement of sending and binding UDP is already done
// by chrome extension permission model.
bool ChromeContentRendererClient::ShouldEnforceWebRTCRoutingPreferences() {
  return !IsStandaloneContentExtensionProcess();
}

GURL ChromeContentRendererClient::OverrideFlashEmbedWithHTML(const GURL& url) {
  if (!url.is_valid())
    return GURL();

  return FlashEmbedRewrite::RewriteFlashEmbedURL(url);
}

std::unique_ptr<content::URLLoaderThrottleProvider>
ChromeContentRendererClient::CreateURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<URLLoaderThrottleProviderImpl>(
      browser_interface_broker_.get(), provider_type, this);
}

blink::WebFrame* ChromeContentRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeExtensionsRendererClient::FindFrame(relative_to_frame, name);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool ChromeContentRendererClient::IsSafeRedirectTarget(const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::RendererExtensionRegistry::Get()->GetByID(url.host());
    if (!extension)
      return false;
    return extensions::WebAccessibleResourcesInfo::IsResourceWebAccessible(
        extension, url.path());
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return true;
}

void ChromeContentRendererClient::DidSetUserAgent(
    const std::string& user_agent) {
#if BUILDFLAG(ENABLE_PRINTING)
  printing::SetAgent(user_agent);
#endif
}

bool ChromeContentRendererClient::RequiresWebComponentsV0(const GURL& url) {
  // TODO(1025782): For now, file:// URLs are allowed to access Web Components
  // v0 features. This will be removed once origin trials support file:// URLs
  // for this purpose.
  return url.SchemeIs(content::kChromeUIScheme) || url.SchemeIs("file");
}
