// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/current_process.h"
#include "base/profiler/thread_group_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
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
#include "chrome/common/crash_keys.h"
#include "chrome/common/profiler/chrome_thread_group_profiler_client.h"
#include "chrome/common/profiler/chrome_thread_profiler_client.h"
#include "chrome/common/profiler/core_unwinders.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/common/webui_util.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/benchmarking_extension.h"
#include "chrome/renderer/browser_exposed_renderer_interfaces.h"
#include "chrome/renderer/chrome_content_settings_agent_delegate.h"
#include "chrome/renderer/chrome_render_frame_observer.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/controlled_frame/controlled_frame_extensions_renderer_api_provider.h"
#include "chrome/renderer/google_accounts_private_api_extension.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/media/flash_embed_rewrite.h"
#include "chrome/renderer/media/webrtc_logging_agent_impl.h"
#include "chrome/renderer/net/net_error_helper.h"
#include "chrome/renderer/net_benchmarking_extension.h"
#include "chrome/renderer/plugins/non_loadable_plugin_placeholder.h"
#include "chrome/renderer/plugins/pdf_plugin_placeholder.h"
#include "chrome/renderer/process_state.h"
#include "chrome/renderer/supervised_user/supervised_user_error_page_controller_delegate_impl.h"
#include "chrome/renderer/trusted_vault_encryption_keys_extension.h"
#include "chrome/renderer/url_loader_throttle_provider_impl.h"
#include "chrome/renderer/v8_unwinder.h"
#include "chrome/renderer/web_link_preview_triggerer_impl.h"
#include "chrome/renderer/websocket_handshake_throttle_provider_impl.h"
#include "chrome/renderer/webui_browser/webui_browser_renderer_extension.h"
#include "chrome/renderer/worker_content_settings_client.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/commerce/content/renderer/commerce_web_extractor.h"
#include "components/content_capture/common/content_capture_features.h"
#include "components/content_capture/renderer/content_capture_sender.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/continuous_search/renderer/search_result_extractor_impl.h"
#include "components/country_codes/country_codes.h"
#include "components/dom_distiller/content/renderer/distillability_agent.h"
#include "components/dom_distiller/content/renderer/distiller_js_render_frame_observer.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/error_page/common/error.h"
#include "components/error_page/common/localized_error.h"
#include "components/feed/feed_feature_list.h"
#include "components/grit/components_scaled_resources.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/history_clusters/core/config.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_client.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_render_frame_observer.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_utils.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/pdf/common/constants.h"
#include "components/pdf/common/pdf_util.h"
#include "components/permissions/features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/renderer/threat_dom_details.h"
#include "components/sampling_profiler/process_type.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "components/security_interstitials/content/renderer/security_interstitial_page_controller_delegate_impl.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "components/wallet/content/renderer/image_extractor.h"
#include "components/wallet/core/common/wallet_features.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "components/webapps/renderer/web_page_metadata_agent.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/renderer/extensions_renderer_api_provider.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_renderer_process_type.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_origin_trials.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/origin.h"
#include "v8/include/v8-isolate.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/renderer/sandbox_status_extension_android.h"
#include "chrome/renderer/wallet/boarding_pass_extractor.h"
#include "components/feed/content/renderer/rss_link_reader.h"
#include "components/feed/feed_feature_list.h"
#else
#include "chrome/renderer/searchbox/searchbox.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "components/search/ntp_features.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
#include "chrome/renderer/media/chrome_speech_recognition_client.h"
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

#if BUILDFLAG(IS_WIN)
#include "chrome/renderer/render_frame_font_family_accessor.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/common/initialize_extensions_client.h"
#include "chrome/renderer/extensions/api/chrome_extensions_renderer_api_provider.h"
#include "chrome/renderer/extensions/chrome_extensions_renderer_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/api/core_extensions_renderer_api_provider.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#include "third_party/blink/public/web/web_settings.h"
#endif  // BUIDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/renderer/internal_plugin_renderer_helpers.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/renderer/printing/chrome_print_render_frame_helper_delegate.h"
#include "components/printing/renderer/print_render_frame_helper.h"  // nogncheck
#include "printing/metafile_agent.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
#include "components/paint_preview/renderer/paint_preview_recorder_impl.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/renderer/spellcheck.h"
#include "components/spellcheck/renderer/spellcheck_provider.h"

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "components/spellcheck/renderer/spellcheck_panel.h"
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#include "chrome/renderer/media/chrome_key_systems.h"
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
using blink::mojom::FetchCacheMode;
using content::RenderFrame;
using content::RenderThread;
using content::WebPluginInfo;
using content::WebPluginMimeType;
using ExtractAllDatalists = autofill::AutofillAgent::ExtractAllDatalists;
using FocusRequiresScroll = autofill::AutofillAgent::FocusRequiresScroll;
using QueryPasswordSuggestions =
    autofill::AutofillAgent::QueryPasswordSuggestions;
using SecureContextRequired = autofill::AutofillAgent::SecureContextRequired;
using UserGestureRequired = autofill::AutofillAgent::UserGestureRequired;
using UsesKeyboardAccessoryForSuggestions =
    autofill::AutofillAgent::UsesKeyboardAccessoryForSuggestions;

namespace {

#if BUILDFLAG(ENABLE_PDF)
std::vector<url::Origin> GetAdditionalPdfInternalPluginAllowedOrigins() {
  return {url::Origin::Create(GURL(chrome::kChromeUIPrintURL))};
}
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PLUGINS)
void AppendParams(
    const std::vector<WebPluginMimeType::Param>& additional_params,
    std::vector<WebString>* existing_names,
    std::vector<WebString>* existing_values) {
  DCHECK(existing_names->size() == existing_values->size());
  size_t existing_size = existing_names->size();
  size_t total_size = existing_size + additional_params.size();

  std::vector<WebString> names(total_size);
  std::vector<WebString> values(total_size);

  for (size_t i = 0; i < existing_size; ++i) {
    names[i] = (*existing_names)[i];
    values[i] = (*existing_values)[i];
  }

  for (size_t i = 0; i < additional_params.size(); ++i) {
    names[existing_size + i] = WebString::FromUTF16(additional_params[i].name);
    values[existing_size + i] =
        WebString::FromUTF16(additional_params[i].value);
  }

  existing_names->swap(names);
  existing_values->swap(values);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

bool IsStandaloneContentExtensionProcess() {
#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return false;
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      extensions::switches::kExtensionProcess);
#endif
}

std::unique_ptr<base::Unwinder> CreateV8Unwinder(v8::Isolate* isolate) {
  return std::make_unique<V8Unwinder>(isolate);
}

}  // namespace

ChromeContentRendererClient::ChromeContentRendererClient()
#if BUILDFLAG(IS_WIN)
    : remote_module_watcher_(nullptr, base::OnTaskRunnerDeleter(nullptr))
#endif
{
  base::ThreadGroupProfiler::SetClient(
      std::make_unique<ChromeThreadGroupProfilerClient>());
  sampling_profiler::ThreadProfiler::SetClient(
      std::make_unique<ChromeThreadProfilerClient>());

  // The profiler can't start before the sandbox is initialized on
  // ChromeOS due to ChromeOS's sandbox initialization code's use of
  // AssertSingleThreaded().
#if !BUILDFLAG(IS_CHROMEOS)
  main_thread_profiler_ =
      sampling_profiler::ThreadProfiler::CreateAndStartOnMainThread();
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  EnsureExtensionsClientInitialized();
  ChromeExtensionsRendererClient::Create();
#endif
}

ChromeContentRendererClient::~ChromeContentRendererClient() = default;

void ChromeContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();

  main_thread_profiler_->SetAuxUnwinderFactory(base::BindRepeating(
      &CreateV8Unwinder, base::Unretained(v8::Isolate::GetCurrent())));

  // In the case of single process mode, the v8 unwinding will not work.
  tracing::TracingSamplerProfiler::SetAuxUnwinderFactoryOnMainThread(
      base::BindRepeating(&CreateV8Unwinder,
                          base::Unretained(v8::Isolate::GetCurrent())));

  const bool is_extension = IsStandaloneContentExtensionProcess();

  if (is_extension) {
    // The process name was set to "Renderer" in RendererMain(). Update it to
    // "Extension Renderer" to highlight that it's hosting an extension.
    base::CurrentProcess::GetInstance().SetProcessType(
        base::CurrentProcessType::PROCESS_RENDERER_EXTENSION);
  }

#if BUILDFLAG(IS_WIN)
  mojo::PendingRemote<mojom::ModuleEventSink> module_event_sink;
  thread->BindHostReceiver(module_event_sink.InitWithNewPipeAndPassReceiver());
  remote_module_watcher_ = RemoteModuleWatcher::Create(
      thread->GetIOTaskRunner(), std::move(module_event_sink));
#endif

  browser_interface_broker_ =
      blink::Platform::Current()->GetBrowserInterfaceBroker();

  chrome_observer_ = std::make_unique<ChromeRenderThreadObserver>();
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  auto* extensions_renderer_client =
      extensions::ExtensionsRendererClient::Get();
  extensions_renderer_client->AddAPIProvider(
      std::make_unique<extensions::CoreExtensionsRendererAPIProvider>());
  extensions_renderer_client->AddAPIProvider(
      std::make_unique<extensions::ChromeExtensionsRendererAPIProvider>());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions_renderer_client->AddAPIProvider(
      std::make_unique<
          controlled_frame::ControlledFrameExtensionsRendererAPIProvider>());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  extensions_renderer_client->RenderThreadStarted();
  WebSecurityPolicy::RegisterURLSchemeAsExtension(
      WebString::FromASCII(extensions::kExtensionScheme));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  WebSecurityPolicy::RegisterURLSchemeAsIsolatedApp(
      WebString::FromASCII(webapps::kIsolatedAppScheme));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
  WebSecurityPolicy::RegisterURLSchemeAsCodeCacheWithHashing(
      WebString::FromASCII(extensions::kExtensionScheme));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  if (!spellcheck_)
    InitSpellCheck();
#endif

  subresource_filter_ruleset_dealer_ =
      std::make_unique<subresource_filter::UnverifiedRulesetDealer>();

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  phishing_model_setter_ =
      std::make_unique<safe_browsing::PhishingModelSetterImpl>();
#endif

  thread->AddObserver(chrome_observer_.get());
  thread->AddObserver(subresource_filter_ruleset_dealer_.get());
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  thread->AddObserver(phishing_model_setter_.get());
#endif

  blink::WebScriptController::RegisterExtension(
      extensions_v8::LoadTimesExtension::Get());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(variations::switches::kEnableBenchmarkingApi)) {
    blink::WebScriptController::RegisterExtension(
        extensions_v8::BenchmarkingExtension::Get());
  }

  if (command_line->HasSwitch(switches::kEnableNetBenchmarking)) {
    blink::WebScriptController::RegisterExtension(
        extensions_v8::NetBenchmarkingExtension::Get());
  }

  // chrome: is also to be permitted to embeds https:// things and have them
  // treated as first-party.
  // See
  // ChromeContentBrowserClient::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel
  WebString chrome_scheme(WebString::FromASCII(content::kChromeUIScheme));
  WebSecurityPolicy::RegisterURLSchemeAsFirstPartyWhenTopLevelEmbeddingSecure(
      chrome_scheme);

  // chrome-native: is a scheme used for placeholder navigations that allow
  // UIs to be drawn with platform native widgets instead of HTML.  These pages
  // should not be accessible.  No code should be runnable in these pages,
  // so it should not need to access anything nor should it allow javascript
  // URLs since it should never be visible to the user.
  // See also ChromeContentClient::AddAdditionalSchemes that adds it as an
  // empty document scheme.
  WebString native_scheme(WebString::FromASCII(chrome::kChromeNativeScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(native_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      native_scheme);

  // chrome-search: and chrome-distiller: pages  should not be accessible by
  // normal content, and should also be unable to script anything but themselves
  // (to help limit the damage that a corrupt page could cause).
  WebString chrome_search_scheme(
      WebString::FromASCII(chrome::kChromeSearchScheme));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  // IWAs can be enabled by either the feature flag or by enterprise
  // policy. In either case the kEnableIsolatedWebAppsInRenderer flag is passed
  // to the renderer process.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableIsolatedWebAppsInRenderer)) {
    // isolated-app: is the scheme used for Isolated Web Apps, which are web
    // applications packaged in Signed Web Bundles.
    WebString isolated_app_scheme(
        WebString::FromASCII(webapps::kIsolatedAppScheme));
    // Resources contained in Isolated Web Apps are HTTP-like and safe to expose
    // to the fetch API.
    WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(
        isolated_app_scheme);
    WebSecurityPolicy::RegisterURLSchemeAsAllowingServiceWorkers(
        isolated_app_scheme);
    WebSecurityPolicy::RegisterURLSchemeAsAllowedForReferrer(
        isolated_app_scheme);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

  // The Instant process can only display the content but not read it. Other
  // processes can't display it or read it. (see http://crbug.com/40309067 for
  // more context on why chrome-search scheme registration is skipped for the
  // instant process).
  bool should_restrict_chrome_search_scheme =
      !command_line->HasSwitch(switches::kInstantProcess);

#if !BUILDFLAG(IS_ANDROID)
  // If the feature is enabled, the `kInstantProcess` command line switch is
  // replaced by the `is_instant_process` flag, which is set later. As a result,
  // we cannot perform chrome-search scheme registration at this stage. This
  // registration will instead be handled in
  // `SetConfigurationOnProcessLockUpdate()` where the instant process flag
  // has been set.
  if (base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer)) {
    should_restrict_chrome_search_scheme = false;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  if (should_restrict_chrome_search_scheme) {
    WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(chrome_search_scheme);
  }

  WebString dom_distiller_scheme(
      WebString::FromASCII(dom_distiller::kDomDistillerScheme));
  // TODO(nyquist): Add test to ensure this happens when the flag is set.
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(dom_distiller_scheme);

#if BUILDFLAG(IS_ANDROID)
  WebSecurityPolicy::RegisterURLSchemeAsAllowedForReferrer(
      WebString::FromUTF8(content::kAndroidAppScheme));
#endif

  // chrome-search: pages should not be accessible by bookmarklets
  // or javascript: URLs typed in the omnibox.
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_search_scheme);

  for (auto& scheme :
       secure_origin_allowlist::GetSchemesBypassingSecureContextCheck()) {
    WebSecurityPolicy::AddSchemeToSecureContextSafelist(
        WebString::FromASCII(scheme));
  }

  // This doesn't work in single-process mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    const auto* heap_profiler_controller =
        heap_profiling::HeapProfilerController::GetInstance();
    // The HeapProfilerController should have been created in
    // ChromeMainDelegate::PostEarlyInitialization.
    CHECK(heap_profiler_controller);
    if (ThreadProfilerConfiguration::Get()
            ->IsProfilerEnabledForCurrentProcess() ||
        heap_profiler_controller->IsEnabled()) {
      sampling_profiler::ThreadProfiler::SetMainThreadTaskRunner(
          base::SingleThreadTaskRunner::GetCurrentDefault());
      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector;
      thread->BindHostReceiver(collector.InitWithNewPipeAndPassReceiver());
      metrics::CallStackProfileBuilder::
          SetParentProfileCollectorForChildProcess(std::move(collector));
    }
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

  new prerender::NoStatePrefetchRenderFrameObserver(render_frame);

  auto content_settings_delegate =
      std::make_unique<ChromeContentSettingsAgentDelegate>(render_frame);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  content_settings_delegate->SetExtensionDispatcher(
      extensions::ExtensionsRendererClient::Get()->dispatcher());
#endif
  content_settings::ContentSettingsAgentImpl* content_settings =
      new content_settings::ContentSettingsAgentImpl(
          render_frame, std::move(content_settings_delegate));
  if (chrome_observer_.get()) {
    if (chrome_observer_->content_settings_manager()) {
      mojo::Remote<content_settings::mojom::ContentSettingsManager> manager;
      chrome_observer_->content_settings_manager()->Clone(
          manager.BindNewPipeAndPassReceiver());
      content_settings->SetContentSettingsManager(std::move(manager));
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()->RenderFrameCreated(render_frame,
                                                                  registry);
#endif

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL) || BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  safe_browsing::ThreatDOMDetails::Create(render_frame, registry);
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<ChromePrintRenderFrameHelperDelegate>());
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
  new paint_preview::PaintPreviewRecorderImpl(render_frame);
#endif

#if BUILDFLAG(IS_ANDROID)
  SandboxStatusExtension::Create(render_frame);
#endif

  TrustedVaultEncryptionKeysExtension::Create(render_frame);
  GoogleAccountsPrivateApiExtension::Create(render_frame);

  if (render_frame->IsMainFrame())
    new webapps::WebPageMetadataAgent(render_frame);

  const bool search_result_extractor_enabled =
      render_frame->IsMainFrame() &&
      page_content_annotations::features::ShouldExtractRelatedSearches();
  if (search_result_extractor_enabled) {
    continuous_search::SearchResultExtractorImpl::Create(render_frame);
  }

  new NetErrorHelper(render_frame);

  new security_interstitials::SecurityInterstitialPageControllerDelegateImpl(
      render_frame);

  new SupervisedUserErrorPageControllerDelegateImpl(render_frame);

  if (!render_frame->IsMainFrame()) {
    auto* main_frame_no_state_prefetch_helper =
        prerender::NoStatePrefetchHelper::Get(
            render_frame->GetMainRenderFrame());
    if (main_frame_no_state_prefetch_helper) {
      // Avoid any race conditions from having the browser tell subframes that
      // they're no-state prefetching.
      new prerender::NoStatePrefetchHelper(
          render_frame,
          main_frame_no_state_prefetch_helper->histogram_prefix());
    }
  }

  // Set up a render frame observer to test if this page is a distiller page.
  new dom_distiller::DistillerJsRenderFrameObserver(
      render_frame, ISOLATED_WORLD_ID_CHROME_INTERNAL);

  if (dom_distiller::ShouldStartDistillabilityService()) {
    // Create DistillabilityAgent to send distillability updates to
    // DistillabilityDriver in the browser process.
    new dom_distiller::DistillabilityAgent(render_frame, DCHECK_IS_ON());
  }

  blink::AssociatedInterfaceRegistry* associated_interfaces =
      render_frame_observer->associated_interfaces();

  if (!render_frame->IsInFencedFrameTree() ||
      base::FeatureList::IsEnabled(blink::features::kFencedFramesAPIChanges)) {
    auto password_autofill_agent = std::make_unique<PasswordAutofillAgent>(
        render_frame, associated_interfaces);
    auto password_generation_agent = std::make_unique<PasswordGenerationAgent>(
        render_frame, password_autofill_agent.get(), associated_interfaces);
    new AutofillAgent(render_frame, std::move(password_autofill_agent),
                      std::move(password_generation_agent),
                      associated_interfaces);
  }

  if (content_capture::features::IsContentCaptureEnabled()) {
    new content_capture::ContentCaptureSender(render_frame,
                                              associated_interfaces);
  }

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  associated_interfaces
      ->AddInterface<extensions::mojom::MimeHandlerViewContainerManager>(
          base::BindRepeating(
              &extensions::MimeHandlerViewContainerManager::BindReceiver,
              base::Unretained(render_frame)));
#endif

  // Owned by |render_frame|.
  new page_load_metrics::MetricsRenderFrameObserver(render_frame);
  // There is no render thread, thus no UnverifiedRulesetDealer in
  // ChromeRenderViewTests.
  if (subresource_filter_ruleset_dealer_) {
    auto* subresource_filter_agent =
        new subresource_filter::SubresourceFilterAgent(
            render_frame, subresource_filter_ruleset_dealer_.get());
    subresource_filter_agent->Initialize();
  }

#if !BUILDFLAG(IS_ANDROID)
  if (process_state::IsInstantProcess() && render_frame->IsMainFrame()) {
    new SearchBox(render_frame);
  }
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
  new SpellCheckProvider(render_frame, spellcheck_.get());

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
  new SpellCheckPanel(render_frame, registry, this);
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)
#endif
#if BUILDFLAG(IS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(feed::switches::kEnableRssLinkReader) &&
      render_frame->IsMainFrame()) {
    new feed::RssLinkReader(render_frame, registry);
  }
#endif

#if BUILDFLAG(IS_WIN)
  if (render_frame->IsMainFrame()) {
    associated_interfaces
        ->AddInterface<chrome::mojom::RenderFrameFontFamilyAccessor>(
            base::BindRepeating(&RenderFrameFontFamilyAccessor::Bind,
                                render_frame));
  }
#endif

  if (render_frame->IsMainFrame()) {
    new commerce::CommerceWebExtractor(render_frame, registry);
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kBoardingPassDetector) &&
      render_frame->IsMainFrame()) {
    new wallet::BoardingPassExtractor(render_frame, registry);
  }
#endif

  if (base::FeatureList::IsEnabled(wallet::kWalletablePassDetection) &&
      render_frame->IsMainFrame()) {
    wallet::ImageExtractor::Create(render_frame, registry);
  }

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebium)) {
    WebUIBrowserRendererExtension::Create(render_frame);
  }
#endif
}

void ChromeContentRendererClient::WebViewCreated(
    blink::WebView* web_view,
    bool was_created_by_renderer,
    const url::Origin* outermost_origin) {
  new prerender::NoStatePrefetchClient(web_view);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()->WebViewCreated(web_view,
                                                              outermost_origin);
#endif
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
#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(ENABLE_PLUGINS)
  DCHECK(plugin_element.HasHTMLTagName("object") ||
         plugin_element.HasHTMLTagName("embed"));

  mojo::AssociatedRemote<chrome::mojom::PluginInfoHost> plugin_info_host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &plugin_info_host);
  // Blink will next try to load a WebPlugin which would end up in
  // OverrideCreatePlugin, sending another IPC only to find out the plugin is
  // not supported. Here it suffices to return false but there should perhaps be
  // a more unified approach to avoid sending the IPC twice.
  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  plugin_info_host->GetPluginInfo(
      original_url, render_frame->GetWebFrame()->Top()->GetSecurityOrigin(),
      mime_type, &plugin_info);
  // TODO(ekaramad): Not continuing here due to a disallowed status should take
  // us to CreatePlugin. See if more in depths investigation of |status| is
  // necessary here (see https://crbug.com/965747). For now, returning false
  // should take us to CreatePlugin after HTMLPlugInElement which is called
  // through HTMLPlugInElement::LoadPlugin code path.
  if (plugin_info->status != chrome::mojom::PluginStatus::kAllowed) {
    // We could get here when a MimeHandlerView is loaded inside a <webview>
    // which is using permissions API (see WebViewPluginTests).
    ChromeExtensionsRendererClient::DidBlockMimeHandlerViewForDisallowedPlugin(
        plugin_element);
    return false;
  }
#if BUILDFLAG(ENABLE_PDF)
  if (plugin_info->actual_mime_type == pdf::kInternalPluginMimeType) {
    // Only actually treat the internal PDF plugin as externally handled if
    // used within an origin allowed to create the internal PDF plugin;
    // otherwise, let Blink try to create the in-process PDF plugin.
    if (IsPdfInternalPluginAllowedOrigin(
            render_frame->GetWebFrame()->GetSecurityOrigin(),
            GetAdditionalPdfInternalPluginAllowedOrigins())) {
      return true;
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)
  return ChromeExtensionsRendererClient::MaybeCreateMimeHandlerView(
      plugin_element, original_url, plugin_info->actual_mime_type,
      plugin_info->plugin);
#else   // !(BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(ENABLE_PLUGINS))
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(ENABLE_PLUGINS)
}

bool ChromeContentRendererClient::IsDomStorageDisabled() const {
  if (!base::FeatureList::IsEnabled(features::kPdfEnforcements)) {
    return false;
  }

#if BUILDFLAG(ENABLE_PDF) && BUILDFLAG(ENABLE_EXTENSIONS)
  // PDF renderers shouldn't need to access DOM storage interfaces. Note that
  // it's still possible to access localStorage or sessionStorage in a PDF
  // document's context via DevTools; returning false here ensures that these
  // objects are just seen as null by JavaScript (similarly to what happens for
  // opaque origins). This avoids a renderer kill by the browser process which
  // isn't expecting PDF renderer processes to ever use DOM storage
  // interfaces. See https://crbug.com/357014503.
  return pdf::IsPdfRenderer();
#else
  return false;
#endif
}

v8::Local<v8::Object> ChromeContentRendererClient::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Used for plugins.
  return extensions::ExtensionsRendererClient::Get()->GetScriptableObject(
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
  // Used for plugins.
  if (!extensions::ExtensionsRendererClient::Get()->OverrideCreatePlugin(
          render_frame, params)) {
    return false;
  }
#endif

  GURL url(params.url);
#if BUILDFLAG(ENABLE_PLUGINS)
  mojo::AssociatedRemote<chrome::mojom::PluginInfoHost> plugin_info_host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(
      &plugin_info_host);

  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  plugin_info_host->GetPluginInfo(
      url, render_frame->GetWebFrame()->Top()->GetSecurityOrigin(),
      orig_mime_type, &plugin_info);
  *plugin = CreatePlugin(render_frame, params, *plugin_info);
#else  // !BUILDFLAG(ENABLE_PLUGINS)
  if (orig_mime_type == pdf::kPDFMimeType) {
    ReportPDFLoadStatus(
        PDFLoadStatus::kShowedDisabledPluginPlaceholderForEmbeddedPdf);

    PDFPluginPlaceholder* placeholder =
        PDFPluginPlaceholder::CreatePDFPlaceholder(render_frame, params);
    *plugin = placeholder->plugin();
    return true;
  }
  auto* placeholder = NonLoadablePluginPlaceholder::CreateNotSupportedPlugin(
      render_frame, params);
  *plugin = placeholder->plugin();

#endif  // BUILDFLAG(ENABLE_PLUGINS)
  return true;
}

bool ChromeContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool has_played_media_before,
    base::OnceClosure closure) {
  return prerender::DeferMediaLoad(render_frame, has_played_media_before,
                                   std::move(closure));
}

#if BUILDFLAG(ENABLE_PLUGINS)

// static
WebPlugin* ChromeContentRendererClient::CreatePlugin(
    content::RenderFrame* render_frame,
    const WebPluginParams& original_params,
    const chrome::mojom::PluginInfo& plugin_info) {
  const WebPluginInfo& info = plugin_info.plugin;
  const std::string& actual_mime_type = plugin_info.actual_mime_type;
  const std::u16string& group_name = plugin_info.group_name;
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
    // Flash has been thoroughly removed in M88+, so we need to have a special
    // case here to display a deprecated message instead of a generic
    // plugin-missing message.
    if (orig_mime_type == "application/x-shockwave-flash" ||
        orig_mime_type == "application/futuresplash") {
      return NonLoadablePluginPlaceholder::CreateFlashDeprecatedPlaceholder(
                 render_frame, original_params)
          ->plugin();
    } else {
      placeholder = ChromePluginPlaceholder::CreateLoadableMissingPlugin(
          render_frame, original_params);
    }
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

    auto* content_settings_agent =
        content_settings::ContentSettingsAgentImpl::Get(render_frame);
    auto* content_settings_agent_delegate =
        ChromeContentSettingsAgentDelegate::Get(render_frame);

    const ContentSettingsType content_type = ContentSettingsType::JAVASCRIPT;

    if ((status == chrome::mojom::PluginStatus::kUnauthorized ||
         status == chrome::mojom::PluginStatus::kBlocked) &&
        content_settings_agent_delegate->IsPluginTemporarilyAllowed(
            identifier)) {
      status = chrome::mojom::PluginStatus::kAllowed;
    }

    auto create_blocked_plugin = [&render_frame, &params, &info, &identifier,
                                  &group_name](int template_id,
                                               const std::u16string& message) {
      return ChromePluginPlaceholder::CreateBlockedPlugin(
          render_frame, params, info, identifier, group_name, template_id,
          message);
    };
    switch (status) {
      case chrome::mojom::PluginStatus::kNotFound: {
        NOTREACHED();
      }
      case chrome::mojom::PluginStatus::kAllowed: {
        if (info.path.value() == ChromeContentClient::kPDFExtensionPluginPath) {
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

        // Delay loading plugins if no-state prefetching.
        // TODO(mmenke):  In the case of NoStatePrefetch, feed into
        //                ChromeContentRendererClient::CreatePlugin instead, to
        //                reduce the chance of future regressions.
        bool is_no_state_prefetching =
            prerender::NoStatePrefetchHelper::IsPrefetching(render_frame);

        if (is_no_state_prefetching) {
          placeholder = ChromePluginPlaceholder::CreateBlockedPlugin(
              render_frame, params, info, identifier, group_name,
              IDR_BLOCKED_PLUGIN_HTML,
              l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
          placeholder->set_blocked_for_no_state_prefetching(
              is_no_state_prefetching);
          placeholder->AllowLoading();
          break;
        }

#if BUILDFLAG(ENABLE_PDF)
        if (info.path.value() == ChromeContentClient::kPDFInternalPluginPath) {
          return pdf::CreateInternalPlugin(
              std::move(params), render_frame,
              GetAdditionalPdfInternalPluginAllowedOrigins());
        }
#endif  // BUILDFLAG(ENABLE_PDF)

        return nullptr;
      }
      case chrome::mojom::PluginStatus::kDisabled: {
        if (info.path.value() == ChromeContentClient::kPDFExtensionPluginPath) {
          ReportPDFLoadStatus(
              PDFLoadStatus::kShowedDisabledPluginPlaceholderForEmbeddedPdf);

          return PDFPluginPlaceholder::CreatePDFPlaceholder(render_frame,
                                                            params)
              ->plugin();
        }

        placeholder = create_blocked_plugin(
            IDR_DISABLED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_DISABLED, group_name));
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
    }
  }
  placeholder->SetStatus(status);
  return placeholder->plugin();
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

void ChromeContentRendererClient::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // TODO(crbug.com/40633267): Get rid of the use of this implementation of
  // |service_manager::LocalInterfaceProvider|. This was done only to avoid
  // churning spellcheck code while eliminting the "chrome" and
  // "chrome_renderer" services. Spellcheck is (and should remain) the only
  // consumer of this implementation.
  RenderThread::Get()->BindHostReceiver(
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
}

void ChromeContentRendererClient::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& web_error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  NetErrorHelper::Get(render_frame)
      ->PrepareErrorPage(
          error_page::Error::NetError(
              web_error.url(), web_error.reason(), web_error.extended_reason(),
              web_error.resolve_error_info(), web_error.has_copy_in_cache()),
          http_method == "POST", std::move(alternative_error_page_info),
          error_html);

  security_interstitials::SecurityInterstitialPageControllerDelegateImpl::Get(
      render_frame)
      ->PrepareForErrorPage();
  SupervisedUserErrorPageControllerDelegateImpl::Get(render_frame)
      ->PrepareForErrorPage();
}

void ChromeContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    int http_status,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  NetErrorHelper::Get(render_frame)
      ->PrepareErrorPage(error_page::Error::HttpError(error.url(), http_status),
                         http_method == "POST",
                         std::move(alternative_error_page_info), error_html);
}

void ChromeContentRendererClient::PostSandboxInitialized() {
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(!main_thread_profiler_);
  main_thread_profiler_ =
      sampling_profiler::ThreadProfiler::CreateAndStartOnMainThread();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ChromeContentRendererClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner* io_thread_task_runner) {
  io_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&sampling_profiler::ThreadProfiler::StartOnChildThread,
                     sampling_profiler::ProfilerThreadType::kIo));
}

void ChromeContentRendererClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* compositor_thread_task_runner) {
  compositor_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&sampling_profiler::ThreadProfiler::StartOnChildThread,
                     sampling_profiler::ProfilerThreadType::kCompositor));
  // Enable stack sampling for tracing.
  // We pass in CreateCoreUnwindersFactory here since it lives in the chrome/
  // layer while TracingSamplerProfiler is outside of chrome/.
  compositor_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&tracing::TracingSamplerProfiler::
                         CreateOnChildThreadWithCustomUnwinders,
                     base::BindRepeating(&CreateCoreUnwindersFactory)));
}

bool ChromeContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kInitIsolateAsForeground) ||
         !IsStandaloneContentExtensionProcess();
}

bool ChromeContentRendererClient::AllowPopup() {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return extensions::ExtensionsRendererClient::Get()->AllowPopup();
#else
  return false;
#endif
}

bool ChromeContentRendererClient::ShouldNotifyServiceWorkerOnWebSocketActivity(
    v8::Local<v8::Context> context) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return extensions::Dispatcher::ShouldNotifyServiceWorkerOnWebSocketActivity(
      context);
#else
  return false;
#endif
}

blink::ProtocolHandlerSecurityLevel
ChromeContentRendererClient::GetProtocolHandlerSecurityLevel(
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return extensions::ExtensionsRendererClient::Get()
      ->GetProtocolHandlerSecurityLevel();
#else
  return blink::ProtocolHandlerSecurityLevel::kStrict;
#endif
}

void ChromeContentRendererClient::WaitForProcessReady() {
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(features::kInstantUsesSpareRenderer)) {
    return;
  }

  bool process_was_ready = chrome_observer_->IsProcessReady();
  bool is_extension = IsStandaloneContentExtensionProcess();
  base::UmaHistogramBoolean(
      is_extension ? "Renderer.ProcessReadyWaitRequired.ExtensionProcess"
                   : "Renderer.ProcessReadyWaitRequired.RegularProcess",
      !process_was_ready);
  if (process_was_ready) {
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  bool ready_within_timeout =
      chrome_observer_->WaitForProcessReady(base::Seconds(5));
  // Add DumpWithoutCrashing() if the process did not become ready after 5
  // seconds. After the timeout, the wait is skipped and execution continues.
  // TODO(http://crbug.com/434977609): Determine whether a crash should be
  // triggered after a timeout, as this may pose a security risk.
  if (!ready_within_timeout) {
    SCOPED_CRASH_KEY_BOOL("WaitForProcessReady", "IsExtensionProcess",
                          is_extension);
    base::debug::DumpWithoutCrashing();
  }

  base::TimeDelta wait_duration = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(
      is_extension ? "Renderer.WaitTimeForProcessReady.ExtensionProcess"
                   : "Renderer.WaitTimeForProcessReady.RegularProcess",
      wait_duration);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeContentRendererClient::WillSendRequest(
    WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& upstream_url,
    const blink::WebURL& target_url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Check whether the request should be allowed. If not allowed, we reset the
  // URL to something invalid to prevent the request and cause an error.
  extensions::ExtensionsRendererClient::Get()->WillSendRequest(
      frame, transition_type, upstream_url, target_url, site_for_cookies,
      initiator_origin, new_url);
  if (!new_url->is_empty())
    return;
#endif

  if (!target_url.ProtocolIs(chrome::kChromeSearchScheme)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  SearchBox* search_box =
      SearchBox::Get(content::RenderFrame::FromWebFrame(frame->LocalRoot()));
  if (search_box) {
    // Note: this GURL copy could be avoided if host() were added to WebURL.
    GURL gurl(target_url);
    if (gurl.host() == chrome::kChromeUIFaviconHost) {
      search_box->GenerateImageURLFromTransientURL(target_url, new_url);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool ChromeContentRendererClient::IsPrefetchOnly(
    content::RenderFrame* render_frame) {
  return prerender::NoStatePrefetchHelper::IsPrefetching(render_frame);
}

uint64_t ChromeContentRendererClient::VisitedLinkHash(
    std::string_view canonical_url) {
  return chrome_observer_->visited_link_reader()->ComputeURLFingerprint(
      canonical_url);
}

uint64_t ChromeContentRendererClient::PartitionedVisitedLinkFingerprint(
    std::string_view canonical_link_url,
    const net::SchemefulSite& top_level_site,
    const url::Origin& frame_origin) {
  return chrome_observer_->visited_link_reader()->ComputePartitionedFingerprint(
      canonical_link_url, top_level_site, frame_origin);
}

bool ChromeContentRendererClient::IsLinkVisited(uint64_t link_hash) {
  return chrome_observer_->visited_link_reader()->IsVisited(link_hash);
}

void ChromeContentRendererClient::AddOrUpdateVisitedLinkSalt(
    const url::Origin& origin,
    uint64_t salt) {
  base::UmaHistogramBoolean(
      "Blink.History.VisitedLinks.IsSaltFromNavigationThrottle", true);
  return chrome_observer_->visited_link_reader()->AddOrUpdateSalt(origin, salt);
}

std::unique_ptr<blink::WebPrescientNetworking>
ChromeContentRendererClient::CreatePrescientNetworking(
    content::RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

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

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
ChromeContentRendererClient::CreateWebSocketHandshakeThrottleProvider() {
  return std::make_unique<WebSocketHandshakeThrottleProviderImpl>(
      browser_interface_broker_.get());
}

bool ChromeContentRendererClient::ShouldUseCodeCacheWithHashing(
    const blink::WebURL& request_url) const {
  if (content::HasWebUIScheme(request_url)) {
    return ShouldUseCodeCacheForWebUIUrl(GURL(request_url));
  }
  return true;
}

std::unique_ptr<media::KeySystemSupportRegistration>
ChromeContentRendererClient::GetSupportedKeySystems(
    content::RenderFrame* render_frame,
    media::GetSupportedKeySystemsCB cb) {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return GetChromeKeySystems(render_frame, std::move(cb));
#else
  std::move(cb).Run({});
  return nullptr;
#endif
}

bool ChromeContentRendererClient::ShouldReportDetailedMessageForSource(
    const std::u16string& source) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
std::unique_ptr<media::SpeechRecognitionClient>
ChromeContentRendererClient::CreateSpeechRecognitionClient(
    content::RenderFrame* render_frame) {
  return std::make_unique<ChromeSpeechRecognitionClient>(render_frame);
}
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

void ChromeContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()->RunScriptsAtDocumentStart(
      render_frame);
  // |render_frame| might be dead by now.
#endif
}

void ChromeContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()->RunScriptsAtDocumentEnd(
      render_frame);
  // |render_frame| might be dead by now.
#endif
}

void ChromeContentRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()->RunScriptsAtDocumentIdle(
      render_frame);
  // |render_frame| might be dead by now.
#endif
}

void ChromeContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // The performance manager service interfaces are provided by the chrome
  // embedder only.
  blink::WebRuntimeFeatures::EnablePerformanceManagerInstrumentation(true);

// Web Share is conditionally enabled here in chrome/, to avoid it
// being made available in WebView or Linux.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
  blink::WebRuntimeFeatures::EnableWebShare(true);
#endif

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillPolicyControlledFeatureAutofill)) {
    blink::WebRuntimeFeatures::EnableAutofill(true);
  }
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillPolicyControlledFeatureManualText)) {
    blink::WebRuntimeFeatures::EnableManualText(true);
  }

  if (base::FeatureList::IsEnabled(subresource_filter::kAdTagging))
    blink::WebRuntimeFeatures::EnableAdTagging(true);

  if (IsStandaloneContentExtensionProcess()) {
    // These Web API features are exposed in extensions.
    blink::WebRuntimeFeatures::EnableWebUSBOnServiceWorkers(true);
#if !BUILDFLAG(IS_ANDROID)
    blink::WebRuntimeFeatures::EnableWebHIDOnServiceWorkers(true);
#endif  // !BUILDFLAG(IS_ANDROID)
    if (blink::WebRuntimeFeatures::IsAIPromptAPIForExtensionEnabled() &&
        base::FeatureList::IsEnabled(
            blink::features::kAIPromptAPIForExtension)) {
      blink::WebRuntimeFeatures::EnableAIPromptAPI(true);
    }
    blink::WebRuntimeFeatures::EnableAIPromptAPIForWorkers(true);
    blink::WebRuntimeFeatures::EnableAIRewriterAPIForWorkers(true);
    blink::WebRuntimeFeatures::EnableAISummarizationAPIForWorkers(true);
    blink::WebRuntimeFeatures::EnableAIWriterAPIForWorkers(true);
    blink::WebRuntimeFeatures::EnableLanguageDetectionAPIForWorkers(true);
    blink::WebRuntimeFeatures::EnableTranslationAPIForWorkers(true);
  }
}

bool ChromeContentRendererClient::AllowScriptExtensionForServiceWorker(
    const url::Origin& script_origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return script_origin.scheme() == extensions::kExtensionScheme;
#else
  return false;
#endif
}

void ChromeContentRendererClient::
    WillInitializeServiceWorkerContextOnWorkerThread() {
  // This is called on the service worker thread.
  sampling_profiler::ThreadProfiler::StartOnChildThread(
      sampling_profiler::ProfilerThreadType::kServiceWorker);
}

void ChromeContentRendererClient::
    DidInitializeServiceWorkerContextOnWorkerThread(
        blink::WebServiceWorkerContextProxy* context_proxy,
        const GURL& service_worker_scope,
        const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()
      ->dispatcher()
      ->DidInitializeServiceWorkerContextOnWorkerThread(
          context_proxy, service_worker_scope, script_url);
#endif
}

void ChromeContentRendererClient::WillEvaluateServiceWorkerOnWorkerThread(
    blink::WebServiceWorkerContextProxy* context_proxy,
    v8::Local<v8::Context> v8_context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url,
    const blink::ServiceWorkerToken& service_worker_token) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()
      ->dispatcher()
      ->WillEvaluateServiceWorkerOnWorkerThread(
          context_proxy, v8_context, service_worker_version_id,
          service_worker_scope, script_url, service_worker_token);
#endif
}

void ChromeContentRendererClient::DidStartServiceWorkerContextOnWorkerThread(
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()
      ->dispatcher()
      ->DidStartServiceWorkerContextOnWorkerThread(
          service_worker_version_id, service_worker_scope, script_url);
#endif
}

void ChromeContentRendererClient::WillDestroyServiceWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context,
    int64_t service_worker_version_id,
    const GURL& service_worker_scope,
    const GURL& script_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionsRendererClient::Get()
      ->dispatcher()
      ->WillDestroyServiceWorkerContextOnWorkerThread(
          context, service_worker_version_id, service_worker_scope, script_url);
#endif
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

std::unique_ptr<blink::URLLoaderThrottleProvider>
ChromeContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return URLLoaderThrottleProviderImpl::Create(provider_type, this,
                                               browser_interface_broker_.get());
}

blink::WebFrame* ChromeContentRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return extensions::ExtensionsRendererClient::FindFrame(relative_to_frame,
                                                         name);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

bool ChromeContentRendererClient::IsSafeRedirectTarget(
    const GURL& upstream_url,
    const GURL& target_url,
    const std::optional<url::Origin>& request_initiator) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (target_url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::RendererExtensionRegistry::Get()->GetByID(
            target_url.GetHost());
    if (!extension) {
      return false;
    }
    if (extensions::WebAccessibleResourcesInfo::IsResourceWebAccessibleRedirect(
            extension, target_url, request_initiator, upstream_url)) {
      return true;
    }
    return extension->guid() == upstream_url.GetHost();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  return true;
}

void ChromeContentRendererClient::DidSetUserAgent(
    const std::string& user_agent) {
#if BUILDFLAG(ENABLE_PRINTING)
  printing::SetAgent(user_agent);
#endif
}

void ChromeContentRendererClient::AppendContentSecurityPolicy(
    const blink::WebURL& url,
    std::vector<blink::WebContentSecurityPolicyHeader>* csp) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#if BUILDFLAG(ENABLE_PDF)
  // Don't apply default CSP to PDF renderers.
  // TODO(crbug.com/40792950): Lock down the CSP once style and script are no
  // longer injected inline by `pdf::PluginResponseWriter`. That class may be a
  // better place to define such CSP, or we may continue doing so here.
  if (pdf::IsPdfRenderer())
    return;
#endif  // BUILDFLAG(ENABLE_PDF)

  DCHECK(csp);
  GURL gurl(url);
  // Use a scoped_refptr to keep the extension alive, since this code can be
  // executed on a worker thread. See https://crbug.com/443038597.
  scoped_refptr<const extensions::Extension> extension =
      extensions::RendererExtensionRegistry::Get()
          ->GetRefCountedExtensionOrAppByURL(gurl);
  if (!extension)
    return;

  // Append a minimum CSP to ensure the extension can't relax the default
  // applied CSP through means like Service Worker.
  const std::string* default_csp =
      extensions::CSPInfo::GetMinimumCSPToAppend(*extension, gurl.GetPath());
  if (!default_csp)
    return;

  csp->push_back({blink::WebString::FromUTF8(*default_csp),
                  network::mojom::ContentSecurityPolicyType::kEnforce,
                  network::mojom::ContentSecurityPolicySource::kHTTP});
#endif
}

std::unique_ptr<blink::WebLinkPreviewTriggerer>
ChromeContentRendererClient::CreateLinkPreviewTriggerer() {
  return ::CreateWebLinkPreviewTriggerer();
}
