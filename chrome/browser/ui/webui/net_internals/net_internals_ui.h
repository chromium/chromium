// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_

#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace network::mojom {
class NetworkContext;
}

class NetInternalsUI;

class NetInternalsUIConfig
    : public content::DefaultWebUIConfig<NetInternalsUI> {
 public:
  NetInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINetInternalsHost) {}
};

class NetInternalsUI : public content::WebUIController {
 public:
  explicit NetInternalsUI(content::WebUI* web_ui);

  NetInternalsUI(const NetInternalsUI&) = delete;
  NetInternalsUI& operator=(const NetInternalsUI&) = delete;

  // This method is used to mock NetworkContext for testing.
  // Specifically this is called by
  // NetInternalsTest::MessageHandler::Set/ResetNetworkContextForTesting.
  static void SetNetworkContextForTesting(
      network::mojom::NetworkContext* network_context_for_testing);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_
