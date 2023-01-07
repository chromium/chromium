// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_CONTROLLER_FACTORY_H_
#define CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_CONTROLLER_FACTORY_H_

#include "chromecast/browser/webui/mojom/webui.mojom.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
class WebUI;
class WebUIController;
}  // namespace content

namespace chromecast {

class CastWebUiControllerFactory : public content::WebUIControllerFactory {
 public:
  CastWebUiControllerFactory(mojo::PendingRemote<mojom::WebUiClient> client,
                             const std::vector<std::string>& hosts);
  CastWebUiControllerFactory(const CastWebUiControllerFactory&) = delete;
  CastWebUiControllerFactory& operator=(const CastWebUiControllerFactory&) =
      delete;
  ~CastWebUiControllerFactory() override;

  // content::WebUIControllerFactory implementation:
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  mojo::Remote<mojom::WebUiClient> client_;
  const std::vector<std::string> hosts_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_CONTROLLER_FACTORY_H_
