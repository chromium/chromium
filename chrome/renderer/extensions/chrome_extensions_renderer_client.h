// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "extensions/renderer/extensions_renderer_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8-local-handle.h"

class GURL;

namespace blink {
enum class ProtocolHandlerSecurityLevel;
class WebElement;
class WebFrame;
class WebLocalFrame;
struct WebPluginParams;
class WebURL;
class WebView;
}

namespace content {
class RenderFrame;
struct WebPluginInfo;
}

namespace extensions {
class Dispatcher;
class RendererPermissionsPolicyDelegate;
class ResourceRequestPolicy;
}

namespace net {
class SiteForCookies;
}

namespace url {
class Origin;
}

namespace ukm {
class MojoUkmRecorder;
}

namespace v8 {
class Isolate;
class Object;
}  // namespace v8

class ChromeExtensionsRendererClient
    : public extensions::ExtensionsRendererClient {
 public:
  ChromeExtensionsRendererClient();

  ChromeExtensionsRendererClient(const ChromeExtensionsRendererClient&) =
      delete;
  ChromeExtensionsRendererClient& operator=(
      const ChromeExtensionsRendererClient&) = delete;

  ~ChromeExtensionsRendererClient() override;

  // Get the LazyInstance for ChromeExtensionsRendererClient.
  static ChromeExtensionsRendererClient* GetInstance();

  // extensions::ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;
  extensions::Dispatcher* GetDispatcher() override;
  void OnExtensionLoaded(const extensions::Extension& extension) override;
  void OnExtensionUnloaded(
      const extensions::ExtensionId& extension_id) override;

  bool ExtensionAPIEnabledForServiceWorkerScript(
      const GURL& scope,
      const GURL& script_url) const override;

  // See ChromeContentRendererClient methods with the same names.
  void RenderThreadStarted();
  void WebViewCreated(blink::WebView* web_view,
                      const url::Origin* outermost_origin);
  void RenderFrameCreated(content::RenderFrame* render_frame,
                          service_manager::BinderRegistry* registry);
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params);
  bool AllowPopup();
  blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel();
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       const net::SiteForCookies& site_for_cookies,
                       const url::Origin* initiator_origin,
                       GURL* new_url);
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate);
  void SetExtensionDispatcherForTest(
      std::unique_ptr<extensions::Dispatcher> extension_dispatcher);
  extensions::Dispatcher* GetExtensionDispatcherForTest();

  static void DidBlockMimeHandlerViewForDisallowedPlugin(
      const blink::WebElement& plugin_element);
  static bool MaybeCreateMimeHandlerView(
      const blink::WebElement& plugin_element,
      const GURL& resource_url,
      const std::string& mime_type,
      const content::WebPluginInfo& plugin_info);
  static blink::WebFrame* FindFrame(blink::WebLocalFrame* relative_to_frame,
                                    const std::string& name);

  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame);

  extensions::Dispatcher* extension_dispatcher() {
    return extension_dispatcher_.get();
  }

 private:
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;
  std::unique_ptr<extensions::Dispatcher> extension_dispatcher_;
  std::unique_ptr<extensions::RendererPermissionsPolicyDelegate>
      permissions_policy_delegate_;
  std::unique_ptr<extensions::ResourceRequestPolicy> resource_request_policy_;
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
