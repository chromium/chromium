// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_model_setter_impl.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/base/key_systems_support_registration.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "v8/include/v8-forward.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/common/conflicts/remote_module_watcher_win.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/common/plugin.mojom.h"
#endif

class ChromeRenderThreadObserver;
#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellCheck;
#endif

namespace sampling_profiler {
class ThreadProfiler;
}  // namespace sampling_profiler

namespace blink {
class WebServiceWorkerContextProxy;
enum class ProtocolHandlerSecurityLevel;
struct WebContentSecurityPolicyHeader;
}

namespace chrome {
class WebRtcLoggingAgentImpl;
}  // namespace chrome

namespace content {
struct WebPluginInfo;
}  // namespace content

#if BUILDFLAG(ENABLE_NACL)
namespace extensions {
class Extension;
}
#endif

namespace fingerprinting_protection_filter {
class UnverifiedRulesetDealer;
}  // namespace fingerprinting_protection_filter

namespace subresource_filter {
class UnverifiedRulesetDealer;
}

namespace url {
class Origin;
}

namespace web_cache {
class WebCacheImpl;
}

class ChromeContentRendererClient
    : public content::ContentRendererClient,
      public service_manager::LocalInterfaceProvider {
 public:
  ChromeContentRendererClient();

  ChromeContentRendererClient(const ChromeContentRendererClient&) = delete;
  ChromeContentRendererClient& operator=(const ChromeContentRendererClient&) =
      delete;

  ~ChromeContentRendererClient() override;

  void RenderThreadStarted() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void WebViewCreated(blink::WebView* web_view,
                      bool was_created_by_renderer,
                      const url::Origin* outermost_origin) override;
  SkBitmap* GetSadPluginBitmap() override;
  SkBitmap* GetSadWebViewBitmap() override;
  bool IsPluginHandledExternally(content::RenderFrame* render_frame,
                                 const blink::WebElement& plugin_element,
                                 const GURL& original_url,
                                 const std::string& mime_type) override;
  bool IsDomStorageDisabled() const override;
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate) override;
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params,
                            blink::WebPlugin** plugin) override;
#if BUILDFLAG(ENABLE_PLUGINS)
  blink::WebPlugin* CreatePluginReplacement(
      content::RenderFrame* render_frame,
      const base::FilePath& plugin_path) override;
#endif
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        content::mojom::AlternativeErrorPageOverrideInfoPtr
                            alternative_error_page_info,
                        std::string* error_html) override;
  void PrepareErrorPageForHttpStatusError(
      content::RenderFrame* render_frame,
      const blink::WebURLError& error,
      const std::string& http_method,
      int http_status,
      content::mojom::AlternativeErrorPageOverrideInfoPtr
          alternative_error_page_info,
      std::string* error_html) override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      base::OnceClosure closure) override;
  void PostSandboxInitialized() override;
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner) override;
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* compositor_thread_task_runner) override;
  bool RunIdleHandlerWhenWidgetsHidden() override;
  bool AllowPopup() override;
  bool ShouldNotifyServiceWorkerOnWebSocketActivity(
      v8::Local<v8::Context> context) override;
  blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel(
      const url::Origin& origin) override;
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& upstream_url,
                       const blink::WebURL& target_url,
                       const net::SiteForCookies& site_for_cookies,
                       const url::Origin* initiator_origin,
                       GURL* new_url) override;
  bool IsPrefetchOnly(content::RenderFrame* render_frame) override;
  uint64_t VisitedLinkHash(std::string_view canonical_url) override;
  uint64_t PartitionedVisitedLinkFingerprint(
      std::string_view canonical_link_url,
      const net::SchemefulSite& top_level_site,
      const url::Origin& frame_origin) override;
  bool IsLinkVisited(uint64_t link_hash) override;
  void AddOrUpdateVisitedLinkSalt(const url::Origin& origin,
                                  uint64_t salt) override;
  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      content::RenderFrame* render_frame) override;
  bool IsExternalPepperPlugin(const std::string& module_name) override;
  bool IsOriginIsolatedPepperPlugin(const base::FilePath& plugin_path) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  bool ShouldUseCodeCacheWithHashing(
      const blink::WebURL& request_url) const override;
  bool ShouldReportDetailedMessageForSource(
      const std::u16string& source) override;
  std::unique_ptr<blink::WebContentSettingsClient>
  CreateWorkerContentSettingsClient(
      content::RenderFrame* render_frame) override;
#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
  std::unique_ptr<media::SpeechRecognitionClient> CreateSpeechRecognitionClient(
      content::RenderFrame* render_frame) override;
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)
  std::unique_ptr<media::KeySystemSupportRegistration> GetSupportedKeySystems(
      content::RenderFrame* render_frame,
      media::GetSupportedKeySystemsCB cb) override;
  bool IsPluginAllowedToUseCameraDeviceAPI(const GURL& url) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  bool AllowScriptExtensionForServiceWorker(
      const url::Origin& script_origin) override;
  void WillInitializeServiceWorkerContextOnWorkerThread() override;
  void DidInitializeServiceWorkerContextOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  void WillEvaluateServiceWorkerOnWorkerThread(
      blink::WebServiceWorkerContextProxy* context_proxy,
      v8::Local<v8::Context> v8_context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url,
      const blink::ServiceWorkerToken& service_worker_token) override;
  void DidStartServiceWorkerContextOnWorkerThread(
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  void WillDestroyServiceWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context,
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url) override;
  bool ShouldEnforceWebRTCRoutingPreferences() override;
  GURL OverrideFlashEmbedWithHTML(const GURL& url) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;
  blink::WebFrame* FindFrame(blink::WebLocalFrame* relative_to_frame,
                             const std::string& name) override;
  bool IsSafeRedirectTarget(const GURL& from_url, const GURL& to_url) override;
  void DidSetUserAgent(const std::string& user_agent) override;
  void AppendContentSecurityPolicy(
      const blink::WebURL& url,
      blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp) override;
  std::unique_ptr<blink::WebLinkPreviewTriggerer> CreateLinkPreviewTriggerer()
      override;

#if BUILDFLAG(ENABLE_PLUGINS)
  static blink::WebPlugin* CreatePlugin(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params,
      const chrome::mojom::PluginInfo& plugin_info);
#endif

#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  static bool IsExtensionOrSharedModuleAllowed(
      const GURL& url,
      const std::set<std::string>& allowlist);
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
  void InitSpellCheck();
#endif

  ChromeRenderThreadObserver* GetChromeObserver() const;
  web_cache::WebCacheImpl* GetWebCache();
  chrome::WebRtcLoggingAgentImpl* GetWebRtcLoggingAgent();

#if BUILDFLAG(ENABLE_SPELLCHECK)
  SpellCheck* GetSpellCheck();
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest, NaClRestriction);
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest,
                           ShouldSuppressErrorPage);

  static GURL GetNaClContentHandlerURL(const std::string& actual_mime_type,
                                       const content::WebPluginInfo& plugin);

  // service_manager::LocalInterfaceProvider:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override;

  void BindWebRTCLoggingAgent(
      mojo::PendingReceiver<chrome::mojom::WebRtcLoggingAgent> receiver);

#if BUILDFLAG(ENABLE_NACL)
  // Determines if a page/app/extension is allowed to run native (non-PNaCl)
  // NaCl modules.
  static bool IsNativeNaClAllowed(const GURL& app_url,
                                  bool is_nacl_unrestricted,
                                  const extensions::Extension* extension);
  static void ReportNaClAppType(bool is_pnacl,
                                const extensions::Extension* extension);
#endif

#if BUILDFLAG(IS_WIN)
  // Observes module load events and notifies the ModuleDatabase in the browser
  // process. This instance is created on the main thread but then lives on the
  // IO task runner.
  RemoteModuleWatcher::UniquePtr remote_module_watcher_;
#endif

  // Used to profile main thread.
  std::unique_ptr<sampling_profiler::ThreadProfiler> main_thread_profiler_;

  std::unique_ptr<ChromeRenderThreadObserver> chrome_observer_;
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;
  std::unique_ptr<chrome::WebRtcLoggingAgentImpl> webrtc_logging_agent_impl_;

#if BUILDFLAG(ENABLE_SPELLCHECK)
  std::unique_ptr<SpellCheck> spellcheck_;
#endif
  std::unique_ptr<subresource_filter::UnverifiedRulesetDealer>
      subresource_filter_ruleset_dealer_;
  std::unique_ptr<fingerprinting_protection_filter::UnverifiedRulesetDealer>
      fingerprinting_protection_ruleset_dealer_;
#if BUILDFLAG(ENABLE_PLUGINS)
  std::set<std::string> allowed_camera_device_origins_;
#endif
  std::unique_ptr<safe_browsing::PhishingModelSetterImpl>
      phishing_model_setter_;

  scoped_refptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      browser_interface_broker_;
};

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
