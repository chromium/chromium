// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "extensions/renderer/extensions_renderer_client.h"

class GURL;

namespace blink {
class WebURL;
}

namespace content {
struct WebPluginInfo;
}

namespace extensions {
class RendererPermissionsPolicyDelegate;
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

  // Creates the global instance of the ChromeExtensionsRendererClient, which
  // will then set itself as the sole ExtensionsRendererClient.
  // Note: This class should be accessed through
  // ExtensionsRendererClient::Get(). Callers should not assume a particular
  // implementation.
  // There's an exception for the static methods below, which just live here
  // for want of a better home.
  static void Create();

  // extensions::ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;

  static void DidBlockMimeHandlerViewForDisallowedPlugin(
      const blink::WebElement& plugin_element);
  static bool MaybeCreateMimeHandlerView(
      const blink::WebElement& plugin_element,
      const GURL& resource_url,
      const std::string& mime_type,
      const content::WebPluginInfo& plugin_info);

 private:
  // extensions::ExtensionsRendererClient implementation.
  void FinishInitialization() override;
  std::unique_ptr<extensions::ResourceRequestPolicy::Delegate>
  CreateResourceRequestPolicyDelegate() override;
  void RecordMetricsForURLRequest(blink::WebLocalFrame* frame,
                                  const blink::WebURL& target_url) override;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;
  std::unique_ptr<extensions::RendererPermissionsPolicyDelegate>
      permissions_policy_delegate_;
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_EXTENSIONS_RENDERER_CLIENT_H_
