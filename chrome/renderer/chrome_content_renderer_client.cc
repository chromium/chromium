// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

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
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/constants.mojom.h"
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
#include "chrome/renderer/app_categorizer.h"
#include "chrome/renderer/benchmarking_extension.h"
#include "chrome/renderer/chrome_render_frame_observer.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/chrome_render_view_observer.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/media/webrtc_logging_message_filter.h"
#include "chrome/renderer/net/net_error_helper.h"
#include "chrome/renderer/net_benchmarking_extension.h"
#include "chrome/renderer/page_load_metrics/metrics_render_frame_observer.h"
#include "chrome/renderer/pepper/pepper_helper.h"
#include "chrome/renderer/plugins/non_loadable_plugin_placeholder.h"
#include "chrome/renderer/plugins/pdf_plugin_placeholder.h"
#include "chrome/renderer/plugins/plugin_preroller.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "chrome/renderer/prerender/prerender_dispatcher.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/prerender/prerenderer_client.h"
#include "chrome/renderer/tts_dispatcher.h"
#include "chrome/renderer/url_loader_throttle_provider_impl.h"
#include "chrome/renderer/websocket_handshake_throttle_provider_impl.h"
#include "chrome/renderer/worker_content_settings_client.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/contextual_search/content/renderer/overlay_js_render_frame_observer.h"
#include "components/data_reduction_proxy/content/renderer/content_previews_render_frame_observer.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/dom_distiller/content/renderer/distillability_agent.h"
#include "components/dom_distiller/content/renderer/distiller_js_render_frame_observer.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/localized_error.h"
#include "components/network_hints/renderer/prescient_networking_dispatcher.h"
#include "components/pdf/renderer/pepper_pdf_host.h"
#include "components/safe_browsing/renderer/threat_dom_details.h"
#include "components/services/heap_profiling/public/cpp/allocator_shim.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/startup_metric_utils/common/startup_metric.mojom.h"
#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/task_scheduler_util/variations_util.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/render_view.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "net/base/net_errors.h"
#include "ppapi/buildflags/buildflags.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/scheduler/renderer_process_type.h"
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
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_security_policy.h"
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

#if defined(FULL_SAFE_BROWSING)
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
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
#include "extensions/renderer/renderer_extension_registry.h"
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

namespace internal {
const char kFlashYouTubeRewriteUMA[] = "Plugin.Flash.YouTubeRewrite";
}  // namespace internal

namespace {

void RecordYouTubeRewriteUMA(internal::YouTubeRewriteStatus status) {
  UMA_HISTOGRAM_ENUMERATION(internal::kFlashYouTubeRewriteUMA, status,
                            internal::NUM_PLUGIN_ERROR);
}

// Whitelist PPAPI for Android Runtime for Chromium. (See crbug.com/383937)
#if BUILDFLAG(ENABLE_PLUGINS)
const char* const kPredefinedAllowedCameraDeviceOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"
};

const char* const kPredefinedAllowedCompositorOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"
};
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
class MediaLoadDeferrer : public content::RenderFrameObserver {
 public:
  MediaLoadDeferrer(content::RenderFrame* render_frame,
                    base::OnceClosure continue_loading_cb)
      : content::RenderFrameObserver(render_frame),
        continue_loading_cb_(std::move(continue_loading_cb)) {}
  ~MediaLoadDeferrer() override {}

 private:
  // content::RenderFrameObserver implementation:
  void WasShown() override {
    std::move(continue_loading_cb_).Run();
    delete this;
  }
  void OnDestruct() override { delete this; }

  base::OnceClosure continue_loading_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaLoadDeferrer);
};

#if defined(OS_WIN)
// Binds |module_event_sink| to the provided |interface_ptr_info|. This function
// is used to do the binding on the IO thread.
void BindModuleEventSink(mojom::ModuleEventSinkPtr* module_event_sink,
                         mojom::ModuleEventSinkPtrInfo interface_ptr_info) {
  DCHECK(!module_event_sink->is_bound());

  module_event_sink->Bind(std::move(interface_ptr_info));
}

// Dispatches a module |event| to the provided |module_event_sink| interface.
// It is expected that this only be called from the IO thread. This is only safe
// because the underlying |module_event_sink| object is never deleted, being
// owned by the leaked ChromeContentRendererClient object. If this ever changes
// then a WeakPtr mechanism would have to be used.
void HandleModuleEventOnIOThread(
    const mojom::ModuleEventSinkPtr& module_event_sink,
    const ModuleWatcher::ModuleEvent& event) {
  DCHECK(module_event_sink.is_bound());

  // Simply send the module load address. The browser can validate this and look
  // up the module details on its own.
  module_event_sink->OnModuleEvent(
      event.event_type, reinterpret_cast<uintptr_t>(event.module_load_address));
}

// Receives notifications from the ModuleWatcher on any thread. Bounces these
// over to the provided |io_task_runner| where they are subsequently dispatched
// to the |module_event_sink| interface.
void OnModuleEvent(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                   const mojom::ModuleEventSinkPtr& module_event_sink,
                   const ModuleWatcher::ModuleEvent& event) {
  // The Mojo interface can only be used from a single thread. Bounce tasks
  // over to it. It is safe to pass an unretained pointer to
  // |module_event_sink|: it is owned by a ChromeContentRendererClient, which is
  // a leaked singleton in the process.
  io_task_runner->PostTask(
      FROM_HERE, base::Bind(&HandleModuleEventOnIOThread,
                            base::ConstRef(module_event_sink), event));
}
#endif

}  // namespace

ChromeContentRendererClient::ChromeContentRendererClient()
    : main_entry_time_(base::TimeTicks::Now()),
      main_thread_profiler_(ThreadProfiler::CreateAndStartOnMainThread()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EnsureExtensionsClientInitialized();
  extensions::ExtensionsRendererClient::Set(
      ChromeExtensionsRendererClient::GetInstance());
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  for (const char* origin : kPredefinedAllowedCameraDeviceOrigins)
    allowed_camera_device_origins_.insert(origin);
  for (const char* origin : kPredefinedAllowedCompositorOrigins)
    allowed_compositor_origins_.insert(origin);
#endif
#if BUILDFLAG(ENABLE_PRINTING)
  printing::SetAgent(GetUserAgent());
#endif

  heap_profiling::SetGCHeapAllocationHookFunctions(
      &blink::WebHeap::SetAllocationHook, &blink::WebHeap::SetFreeHook);
}

ChromeContentRendererClient::~ChromeContentRendererClient() = default;

void ChromeContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();

  thread->SetRendererProcessType(
      IsStandaloneContentExtensionProcess()
          ? blink::scheduler::RendererProcessType::kExtensionRenderer
          : blink::scheduler::RendererProcessType::kRenderer);

  {
    startup_metric_utils::mojom::StartupMetricHostPtr startup_metric_host;
    GetConnector()->BindInterface(chrome::mojom::kServiceName,
                                  &startup_metric_host);
    startup_metric_host->RecordRendererMainEntryTime(main_entry_time_);
  }

#if defined(OS_WIN)
  // Bind the ModuleEventSink interface.
  thread->GetConnector()->BindInterface(content::mojom::kBrowserServiceName,
                                        &module_event_sink_);

  // Rebind the ModuleEventSink to the IO task runner.
  // The use of base::Unretained() is safe here because |module_event_sink_|
  // is never deleted and is only used on the IO task runner.
  thread->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&BindModuleEventSink,
                                base::Unretained(&module_event_sink_),
                                module_event_sink_.PassInterface()));

  // It is safe to pass an unretained pointer to |module_event_sink_|, as it
  // is owned by the process singleton ChromeContentRendererClient, which is
  // leaked.
  module_watcher_ = ModuleWatcher::Create(
      base::BindRepeating(&OnModuleEvent, thread->GetIOTaskRunner(),
                          base::ConstRef(module_event_sink_)));
#endif

  chrome_observer_.reset(new ChromeRenderThreadObserver());
  web_cache_impl_.reset(new web_cache::WebCacheImpl());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()->RenderThreadStarted();
#endif

  prescient_networking_dispatcher_.reset(
      new network_hints::PrescientNetworkingDispatcher());
#if BUILDFLAG(ENABLE_SPELLCHECK)
  if (!spellcheck_)
    InitSpellCheck();
#endif
#if defined(FULL_SAFE_BROWSING)
  registry_.AddInterface(
      base::BindRepeating(&safe_browsing::PhishingClassifierFilter::Create));
#endif
  prerender_dispatcher_.reset(new prerender::PrerenderDispatcher());
  subresource_filter_ruleset_dealer_.reset(
      new subresource_filter::UnverifiedRulesetDealer());
  webrtc_logging_message_filter_ =
      new WebRtcLoggingMessageFilter(thread->GetIOTaskRunner());

  thread->AddObserver(chrome_observer_.get());
  thread->AddObserver(prerender_dispatcher_.get());
  thread->AddObserver(subresource_filter_ruleset_dealer_.get());

#if !defined(OS_ANDROID)
  thread->AddObserver(SearchBouncer::GetInstance());
#endif

  thread->AddFilter(webrtc_logging_message_filter_.get());
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
       secure_origin_whitelist::GetWhitelist()) {
    WebSecurityPolicy::AddOriginTrustworthyWhiteList(
        WebString::FromUTF8(origin_or_hostname_pattern));
  }

  for (auto& scheme :
       secure_origin_whitelist::GetSchemesBypassingSecureContextCheck()) {
    WebSecurityPolicy::AddSchemeToBypassSecureContextWhitelist(
        WebString::FromASCII(scheme));
  }

  main_thread_profiler_->SetMainThreadTaskRunner(
      base::ThreadTaskRunnerHandle::Get());
  ThreadProfiler::SetServiceManagerConnectorForChildProcess(
      thread->GetConnector());
}

void ChromeContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  ChromeRenderFrameObserver* render_frame_observer =
      new ChromeRenderFrameObserver(render_frame);
  service_manager::BinderRegistry* registry = render_frame_observer->registry();

  bool should_whitelist_for_content_settings =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInstantProcess);
  ContentSettingsObserver* content_settings = new ContentSettingsObserver(
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

#if defined(FULL_SAFE_BROWSING) || defined(SAFE_BROWSING_DB_REMOTE)
  safe_browsing::ThreatDOMDetails::Create(render_frame, registry);
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<ChromePrintRenderFrameHelperDelegate>());
#endif

#if defined(OS_ANDROID)
  SandboxStatusExtension::Create(render_frame);
#endif

  new NetErrorHelper(render_frame);

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

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDistillabilityService)) {
    // Create DistillabilityAgent to send distillability updates to
    // DistillabilityDriver in the browser process.
    new dom_distiller::DistillabilityAgent(render_frame);
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

#if !defined(OS_ANDROID)
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

  new ChromeRenderViewObserver(render_view, web_cache_impl_.get());
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

bool ChromeContentRendererClient::MaybeCreateMimeHandlerView(
    content::RenderFrame* render_frame,
    const blink::WebElement& plugin_element,
    const GURL& original_url,
    const std::string& mime_type,
    int32_t instance_id_to_use) {
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
  if (plugin_info->status == chrome::mojom::PluginStatus::kNotFound ||
      !ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
          plugin_element, original_url, plugin_info->actual_mime_type,
          plugin_info->plugin, instance_id_to_use)) {
    return false;
  }
  return true;
#else
  return false;
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
  // Don't allow autoplay/autoload of media resources in a RenderFrame that is
  // hidden and has never played any media before.  We want to allow future
  // loads even when hidden to allow playlist-like functionality.
  //
  // NOTE: This is also used to defer media loading for prerender.
  if ((render_frame->IsHidden() && !has_played_media_before) ||
      prerender::PrerenderHelper::IsPrerendering(render_frame)) {
    new MediaLoadDeferrer(render_frame, std::move(closure));
    return true;
  }

  std::move(closure).Run();
  return false;
}

#if BUILDFLAG(ENABLE_PLUGINS)

chrome::mojom::PluginInfoHostAssociatedPtr&
ChromeContentRendererClient::GetPluginInfoHost() {
  struct PluginInfoHostHolder {
    PluginInfoHostHolder() {
      RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
          &plugin_info_host);
    }
    ~PluginInfoHostHolder() {}
    chrome::mojom::PluginInfoHostAssociatedPtr plugin_info_host;
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
  ChromePluginPlaceholder* placeholder = NULL;

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

    ContentSettingsObserver* observer =
        ContentSettingsObserver::Get(render_frame);

    const ContentSettingsType content_type =
        ShouldUseJavaScriptSettingForPlugin(info)
            ? CONTENT_SETTINGS_TYPE_JAVASCRIPT
            : CONTENT_SETTINGS_TYPE_PLUGINS;

    if ((status == chrome::mojom::PluginStatus::kUnauthorized ||
         status == chrome::mojom::PluginStatus::kBlocked) &&
        observer->IsPluginTemporarilyAllowed(identifier)) {
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
          bool is_nacl_unrestricted = false;
          if (is_nacl_mime_type) {
            is_nacl_unrestricted =
                base::CommandLine::ForCurrentProcess()->HasSwitch(
                    switches::kEnableNaCl);
          } else if (is_pnacl_mime_type) {
            is_nacl_unrestricted = true;
          }
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
          const Extension* extension =
              extensions::RendererExtensionRegistry::Get()
                  ->GetExtensionOrAppByURL(manifest_url);
          if (!IsNaClAllowed(manifest_url,
                             app_url,
                             is_nacl_unrestricted,
                             extension,
                             &params)) {
            WebString error_message;
            if (is_nacl_mime_type) {
              error_message =
                  "Only unpacked extensions and apps installed from the Chrome "
                  "Web Store can load NaCl modules without enabling Native "
                  "Client in about:flags.";
            } else if (is_pnacl_mime_type) {
              error_message =
                  "Portable Native Client must not be disabled in about:flags.";
            }
            frame->AddMessageToConsole(WebConsoleMessage(
                WebConsoleMessage::kLevelError, error_message));
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

        // Report PDF load metrics. Since the PDF plugin is comprised of an
        // extension that loads a second plugin, avoid double counting by
        // ignoring the creation of the second plugin.
        if (info.name ==
                ASCIIToUTF16(ChromeContentClient::kPDFExtensionPluginName) &&
            GURL(frame->GetDocument().Url()).host_piece() !=
                extension_misc::kPdfExtensionId) {
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
        chrome::mojom::PluginHostAssociatedPtr plugin_host;
        render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
            &plugin_host);
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
        chrome::mojom::PluginAuthHostAssociatedPtr plugin_auth_host;
        render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
            &plugin_auth_host);
        plugin_auth_host->BlockedUnauthorizedPlugin(group_name, identifier);
        observer->DidBlockContentType(content_type, group_name);
        break;
      }
      case chrome::mojom::PluginStatus::kBlocked: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
        placeholder->AllowLoading();
        RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Blocked"));
        observer->DidBlockContentType(content_type, group_name);
        break;
      }
      case chrome::mojom::PluginStatus::kBlockedByPolicy: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED_BY_POLICY,
                                       group_name));
        RenderThread::Get()->RecordAction(
            UserMetricsAction("Plugin_BlockedByPolicy"));
        observer->DidBlockContentType(content_type, group_name);
        break;
      }
      case chrome::mojom::PluginStatus::kBlockedNoLoading: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED_NO_LOADING,
                                       group_name));
        observer->DidBlockContentType(content_type, group_name);
        break;
      }
      case chrome::mojom::PluginStatus::kComponentUpdateRequired: {
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        placeholder->AllowLoading();
        chrome::mojom::PluginHostAssociatedPtr plugin_host;
        render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
            &plugin_host);
        plugin_host->BlockedComponentUpdatedPlugin(
            placeholder->BindPluginRenderer(), identifier);
        break;
      }

      case chrome::mojom::PluginStatus::kRestartRequired: {
#if defined(OS_LINUX)
        placeholder = create_blocked_plugin(
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_RESTART_REQUIRED,
                                       group_name));
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

void ChromeContentRendererClient::OnStart() {
  context()->connector()->BindConnectorRequest(std::move(connector_request_));
}

void ChromeContentRendererClient::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& name,
    mojo::ScopedMessagePipeHandle handle) {
  registry_.TryBindInterface(name, &handle);
}

void ChromeContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // In some tests, this may not be configured.
  if (!connector_)
    return;
  connector_->BindInterface(
      service_manager::Identity(chrome::mojom::kServiceName), interface_name,
      std::move(interface_pipe));
}

#if BUILDFLAG(ENABLE_NACL)
//  static
bool ChromeContentRendererClient::IsNaClAllowed(
    const GURL& manifest_url,
    const GURL& app_url,
    bool is_nacl_unrestricted,
    const Extension* extension,
    WebPluginParams* params) {
  // Temporarily allow these whitelisted apps and WebUIs to use NaCl.
  bool is_whitelisted_web_ui =
      app_url.spec() == chrome::kChromeUIAppListStartPageURL;

  bool is_invoked_by_webstore_installed_extension = false;
  bool is_extension_unrestricted = false;
  bool is_extension_force_installed = false;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool is_extension_from_webstore = extension && extension->from_webstore();

  bool is_invoked_by_extension = app_url.SchemeIs(extensions::kExtensionScheme);
  bool is_invoked_by_hosted_app = extension &&
      extension->is_hosted_app() &&
      extension->web_extent().MatchesURL(app_url);

  is_invoked_by_webstore_installed_extension = is_extension_from_webstore &&
      (is_invoked_by_extension || is_invoked_by_hosted_app);

  // Allow built-in extensions and developer mode extensions.
  is_extension_unrestricted = extension &&
       (extensions::Manifest::IsUnpackedLocation(extension->location()) ||
        extensions::Manifest::IsComponentLocation(extension->location()));
  // Allow extensions force installed by admin policy.
  is_extension_force_installed = extension &&
       extensions::Manifest::IsPolicyLocation(extension->location());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Allow NaCl under any of the following circumstances:
  //  1) An app or URL is explictly whitelisted above.
  //  2) An extension is loaded unpacked or built-in (component) to Chrome.
  //  3) An extension is force installed by policy.
  //  4) An extension is installed from the webstore, and invoked in that
  //     context (hosted app URL or chrome-extension:// scheme).
  //  5) --enable-nacl is set.
  bool is_nacl_allowed_by_location =
      is_whitelisted_web_ui ||
      AppCategorizer::IsWhitelistedApp(manifest_url, app_url) ||
      is_extension_unrestricted ||
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
    const WebURLRequest& failed_request,
    const blink::WebURLError& web_error,
    std::string* error_html,
    base::string16* error_description) {
  PrepareErrorPageInternal(
      render_frame, failed_request,
      error_page::Error::NetError(web_error.url(), web_error.reason(),
                                  web_error.has_copy_in_cache()),
      error_html, error_description);
}

void ChromeContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const WebURLRequest& failed_request,
    const GURL& unreachable_url,
    int http_status,
    std::string* error_html,
    base::string16* error_description) {
  PrepareErrorPageInternal(
      render_frame, failed_request,
      error_page::Error::HttpError(unreachable_url, http_status), error_html,
      error_description);
}

void ChromeContentRendererClient::GetErrorDescription(
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    base::string16* error_description) {
  GetErrorDescriptionInternal(
      failed_request,
      error_page::Error::NetError(error.url(), error.reason(),
                                  error.has_copy_in_cache()),
      error_description);
}

void ChromeContentRendererClient::PrepareErrorPageInternal(
    content::RenderFrame* render_frame,
    const WebURLRequest& failed_request,
    const error_page::Error& error,
    std::string* error_html,
    base::string16* error_description) {
  bool is_post = failed_request.HttpMethod().Ascii() == "POST";
  bool is_ignoring_cache =
      failed_request.GetCacheMode() == FetchCacheMode::kBypassCache;
  NetErrorHelper::Get(render_frame)
      ->PrepareErrorPage(error, is_post, is_ignoring_cache, error_html);
  if (error_description)
    GetErrorDescriptionInternal(failed_request, error, error_description);
}

void ChromeContentRendererClient::GetErrorDescriptionInternal(
    const blink::WebURLRequest& failed_request,
    const error_page::Error& error,
    base::string16* error_description) {
  bool is_post = failed_request.HttpMethod().Ascii() == "POST";
  if (error_description) {
    *error_description = error_page::LocalizedError::GetErrorDetails(
        error.domain(), error.reason(), is_post);
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

bool ChromeContentRendererClient::ShouldFork(WebLocalFrame* frame,
                                             const GURL& url,
                                             const std::string& http_method,
                                             bool is_initial_navigation,
                                             bool is_server_redirect) {
  DCHECK(!frame->Parent());

#if !defined(OS_ANDROID)
  // If this is the Instant process, fork all navigations originating from the
  // renderer.  The destination page will then be bucketed back to this Instant
  // process if it is an Instant url, or to another process if not.  Conversely,
  // fork if this is a non-Instant process navigating to an Instant url, so that
  // such navigations can also be bucketed into an Instant renderer.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInstantProcess) ||
      SearchBouncer::GetInstance()->ShouldFork(url)) {
    return true;
  }
#endif

  // TODO(lukasza): https://crbug.com/650694: For now, we skip the rest for POST
  // submissions.  This is because 1) in M54 there are some remaining issues
  // with POST in OpenURL path (e.g. https://crbug.com/648648) and 2) OpenURL
  // path regresses (blocks) navigations that result in downloads
  // (https://crbug.com/646261).  In the long-term we should avoid forking for
  // extensions (not hosted apps though) altogether and rely on transfers logic
  // instead.
  if (http_method != "GET")
    return false;

  // If |url| matches one of the prerendered URLs, stop this navigation and try
  // to swap in the prerendered page on the browser process. If the prerendered
  // page no longer exists by the time the OpenURL IPC is handled, a normal
  // navigation is attempted.
  if (prerender_dispatcher_.get() &&
      prerender_dispatcher_->IsPrerenderURL(url)) {
    return true;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool should_fork = ChromeExtensionsRendererClient::ShouldFork(
      frame, url, is_initial_navigation, is_server_redirect);
  if (should_fork)
    return true;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return false;
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
    SearchBox::ImageSourceType type = SearchBox::NONE;
    if (gurl.host_piece() == chrome::kChromeUIFaviconHost)
      type = SearchBox::FAVICON;
    else if (gurl.host_piece() == chrome::kChromeUIThumbnailHost)
      type = SearchBox::THUMB;

    if (type != SearchBox::NONE)
      search_box->GenerateImageURLFromTransientURL(url, type, new_url);
  }
#endif  // !defined(OS_ANDROID)
}

bool ChromeContentRendererClient::IsPrefetchOnly(
    content::RenderFrame* render_frame,
    const blink::WebURLRequest& request) {
  return prerender::PrerenderHelper::GetPrerenderMode(render_frame) ==
         prerender::PREFETCH_ONLY;
}

unsigned long long ChromeContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return chrome_observer_->visited_link_slave()->ComputeURLFingerprint(
      canonical_url, length);
}

bool ChromeContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return chrome_observer_->visited_link_slave()->IsVisited(link_hash);
}

blink::WebPrescientNetworking*
ChromeContentRendererClient::GetPrescientNetworking() {
  return prescient_networking_dispatcher_.get();
}

bool ChromeContentRendererClient::ShouldOverridePageVisibilityState(
    const content::RenderFrame* render_frame,
    blink::mojom::PageVisibilityState* override_state) {
  if (!prerender::PrerenderHelper::IsPrerendering(render_frame))
    return false;

  *override_state = blink::mojom::PageVisibilityState::kPrerender;
  return true;
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
  spellcheck_ = std::make_unique<SpellCheck>(&registry_, this);
}
#endif

std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
ChromeContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<WebSocketHandshakeThrottleProviderImpl>();
}

std::unique_ptr<blink::WebSpeechSynthesizer>
ChromeContentRendererClient::OverrideSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return std::make_unique<TtsDispatcher>(client);
}

void ChromeContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
  key_systems_provider_.AddSupportedKeySystems(key_systems);
}

bool ChromeContentRendererClient::IsKeySystemsUpdateNeeded() {
  return key_systems_provider_.IsKeySystemsUpdateNeeded();
}

bool ChromeContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) const {
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

bool ChromeContentRendererClient::IsPluginAllowedToUseCompositorAPI(
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePepperTesting))
    return true;
  if (IsExtensionOrSharedModuleWhitelisted(url, allowed_compositor_origins_))
    return true;

  version_info::Channel channel = chrome::GetChannel();
  return channel <= version_info::Channel::DEV;
#else
  return false;
#endif
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
  if (!rappor_recorder_) {
    RenderThread::Get()->GetConnector()->BindInterface(
        content::mojom::kBrowserServiceName, &rappor_recorder_);
  }
  rappor_recorder_->RecordRappor(metric, sample);
}

void ChromeContentRendererClient::RecordRapporURL(const std::string& metric,
                                                  const GURL& url) {
  if (!rappor_recorder_) {
    RenderThread::Get()->GetConnector()->BindInterface(
        content::mojom::kBrowserServiceName, &rappor_recorder_);
  }
  rappor_recorder_->RecordRapporURL(metric, url);
}

void ChromeContentRendererClient::AddImageContextMenuProperties(
    const WebURLResponse& response,
    bool is_image_in_context_a_placeholder_image,
    std::map<std::string, std::string>* properties) {
  DCHECK(properties);

  WebString cpct_value = response.HttpHeaderField(WebString::FromASCII(
      data_reduction_proxy::chrome_proxy_content_transform_header()));
  WebString chrome_proxy_value = response.HttpHeaderField(
      WebString::FromASCII(data_reduction_proxy::chrome_proxy_header()));

  if (is_image_in_context_a_placeholder_image ||
      data_reduction_proxy::IsEmptyImagePreview(cpct_value.Utf8(),
                                                chrome_proxy_value.Utf8())) {
    (*properties)
        [data_reduction_proxy::chrome_proxy_content_transform_header()] =
            data_reduction_proxy::empty_image_directive();
  }
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
// Web Share is shipped on Android, experimental otherwise. It is enabled here,
// in chrome/, to avoid it being made available in other clients of content/
// that do not have a Web Share Mojo implementation.
#if defined(OS_ANDROID)
  blink::WebRuntimeFeatures::EnableWebShare(true);
#endif

  if (base::FeatureList::IsEnabled(subresource_filter::kAdTagging))
    blink::WebRuntimeFeatures::EnableAdTagging(true);
}

void ChromeContentRendererClient::
    DidInitializeServiceWorkerContextOnWorkerThread(
        v8::Local<v8::Context> context,
        int64_t service_worker_version_id,
        const GURL& service_worker_scope,
        const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeExtensionsRendererClient::GetInstance()
      ->extension_dispatcher()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id, service_worker_scope, script_url);
#endif
}

void ChromeContentRendererClient::DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::Dispatcher::DidStartServiceWorkerContextOnWorkerThread(
      service_worker_version_id, service_worker_scope, script_url);
#endif
}

void ChromeContentRendererClient::WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::Dispatcher::WillDestroyServiceWorkerContextOnWorkerThread(
      context, service_worker_version_id, service_worker_scope, script_url);
#endif
}

bool ChromeContentRendererClient::IsExcludedHeaderForServiceWorkerFetchEvent(
    const std::string& header_name) {
  return header_name == variations::kClientDataHeader;
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

  // We'll only modify YouTube Flash embeds. The URLs can be recognized since
  // they're in the following form: youtube.com/v/VIDEO_ID. So, we check to see
  // if the given URL does follow that format.
  if (!url.DomainIs("youtube.com") && !url.DomainIs("youtube-nocookie.com"))
    return GURL();
  if (url.path().find("/v/") != 0)
    return GURL();

  std::string url_str = url.spec();
  internal::YouTubeRewriteStatus result = internal::NUM_PLUGIN_ERROR;

  // If the website is using an invalid YouTube URL, we'll try and
  // fix the URL by ensuring that if there are multiple parameters,
  // the parameter string begins with a "?" and then follows with a "&"
  // for each subsequent parameter. We do this because the Flash video player
  // has some URL correction capabilities so we don't want this move to HTML5
  // to break webpages that used to work.
  size_t index = url_str.find_first_of("&?");
  bool invalid_url = index != std::string::npos && url_str.at(index) == '&';

  if (invalid_url) {
    // ? should appear first before all parameters
    url_str.replace(index, 1, "?");

    // Replace all instances of ? (after the first) with &
    for (size_t pos = index + 1;
         (pos = url_str.find("?", pos)) != std::string::npos; pos += 1) {
      url_str.replace(pos, 1, "&");
    }
  }

  GURL corrected_url = GURL(url_str);
  // Chrome used to only rewrite embeds with enablejsapi=1 on mobile for
  // backward compatibility but with Flash embeds deprecated by YouTube, they
  // are rewritten on all platforms. However, a different result is used in
  // order to keep track of how popular they are.
  if (corrected_url.query().find("enablejsapi=1") != std::string::npos)
    result = internal::SUCCESS_ENABLEJSAPI;

  // Change the path to use the YouTube HTML5 API
  std::string path = corrected_url.path();
  path.replace(path.find("/v/"), 3, "/embed/");

  url::Replacements<char> r;
  r.SetPath(path.c_str(), url::Component(0, path.length()));

  if (result == internal::NUM_PLUGIN_ERROR)
    result = invalid_url ? internal::SUCCESS_PARAMS_REWRITE : internal::SUCCESS;

  RecordYouTubeRewriteUMA(result);
  return corrected_url.ReplaceComponents(r);
}

std::unique_ptr<base::TaskScheduler::InitParams>
ChromeContentRendererClient::GetTaskSchedulerInitParams() {
  return task_scheduler_util::GetTaskSchedulerInitParamsForRenderer();
}

bool ChromeContentRendererClient::OverrideLegacySymantecCertConsoleMessage(
    const GURL& url,
    std::string* console_message) {
  *console_message = base::StringPrintf(
      "The SSL certificate used to load resources from %s"
      " will be distrusted very soon. Once distrusted, users will be prevented"
      " from loading these resources. See https://g.co/chrome/symantecpkicerts"
      " for more information.",
      url::Origin::Create(url).Serialize().c_str());
  return true;
}

void ChromeContentRendererClient::CreateRendererService(
    service_manager::mojom::ServiceRequest service_request) {
  service_context_ = std::make_unique<service_manager::ServiceContext>(
      std::make_unique<service_manager::ForwardingService>(this),
      std::move(service_request));
}

service_manager::Connector* ChromeContentRendererClient::GetConnector() {
  if (!connector_)
    connector_ = service_manager::Connector::Create(&connector_request_);
  return connector_.get();
}

std::unique_ptr<content::URLLoaderThrottleProvider>
ChromeContentRendererClient::CreateURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<URLLoaderThrottleProviderImpl>(provider_type, this);
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
