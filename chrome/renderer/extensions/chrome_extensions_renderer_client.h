// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "extensions/renderer/extensions_renderer_client.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace blink {
class WebLocalFrame;
class WebURL;
}

namespace content {
struct WebPluginInfo;
}

namespace extensions {
class RendererPermissionsPolicyDelegate;
class ResourceRequestPolicy;
}

namespace net {
class SiteForCookies;
}

namespace ukm {
class MojoUkmRecorder;
}

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
  void RenderThreadStarted() override;
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;
  void OnExtensionLoaded(const extensions::Extension& extension) override;
  void OnExtensionUnloaded(
      const extensions::ExtensionId& extension_id) override;

  // See ChromeContentRendererClient methods with the same names.
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& upstream_url,
                       const blink::WebURL& target_url,
                       const net::SiteForCookies& site_for_cookies,
                       const url::Origin* initiator_origin,
                       GURL* new_url);

  static void DidBlockMimeHandlerViewForDisallowedPlugin(
      const blink::WebElement& plugin_element);
  static bool MaybeCreateMimeHandlerView(
      const blink::WebElement& plugin_element,
      const GURL& resource_url,
      const std::string& mime_type,
      const content::WebPluginInfo& plugin_info);

 private:
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;
  std::unique_ptr<extensions::RendererPermissionsPolicyDelegate>
      permissions_policy_delegate_;
  std::unique_ptr<extensions::ResourceRequestPolicy> resource_request_policy_;
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
