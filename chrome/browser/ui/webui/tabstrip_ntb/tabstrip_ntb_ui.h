// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TABSTRIP_NTB_TABSTRIP_NTB_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TABSTRIP_NTB_TABSTRIP_NTB_UI_H_

#include <string_view>

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs_api {
class TabStripService;
}  // namespace tabs_api

namespace tabstrip_ntb {

class TabStripNtbUI;

class TabStripNtbUIConfig : public DefaultTopChromeWebUIConfig<TabStripNtbUI> {
 public:
  TabStripNtbUIConfig()
      : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                    chrome::kChromeUITabStripNtbHost) {}

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUIController for chrome://tabstrip-ntb.top-chrome/.
class TabStripNtbUI : public TopChromeWebUIController {
 public:
  explicit TabStripNtbUI(content::WebUI* web_ui);
  TabStripNtbUI(const TabStripNtbUI&) = delete;
  TabStripNtbUI& operator=(const TabStripNtbUI&) = delete;
  ~TabStripNtbUI() override;

  static constexpr std::string_view GetWebUIName() { return "TabStripNtb"; }

  // Sets the `tabs_api::TabStripService` that the page hosted in `web_contents`
  // talks to. Calls the page makes before the service is set are queued and
  // delivered once set. `service` must outlive `web_contents`, or the embedder
  // must call this with nullptr before destroying it.
  static void SetTabStripServiceForWebContents(
      content::WebContents* web_contents,
      tabs_api::TabStripService* service);

  void BindInterface(
      mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver);

 private:
  // The page end of a pipe to the `tabs_api::TabStripService`, created at
  // construction. `BindInterface()` fuses the page's receiver with it. The
  // other end goes to the service as soon as there is one (see
  // `SetTabStripServiceForWebContents()`).
  mojo::PendingRemote<tabs_api::mojom::TabStripService> tab_strip_client_end_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace tabstrip_ntb

#endif  // CHROME_BROWSER_UI_WEBUI_TABSTRIP_NTB_TABSTRIP_NTB_UI_H_
