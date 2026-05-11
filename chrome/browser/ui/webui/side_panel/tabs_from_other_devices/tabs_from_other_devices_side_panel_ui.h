// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_UI_H_

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom-forward.h"

class BrowserWindowInterface;
class TabsFromOtherDevicesSidePanelMetrics;
class TabsFromOtherDevicesSidePanelUI;

namespace browser_sync {
class ForeignSessionHandler;
}  // namespace browser_sync

class TabsFromOtherDevicesUIConfig
    : public DefaultTopChromeWebUIConfig<TabsFromOtherDevicesSidePanelUI> {
 public:
  TabsFromOtherDevicesUIConfig();

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::optional<int> GetCommandIdForTesting() override;
};

class TabsFromOtherDevicesSidePanelUI : public TopChromeWebUIController {
 public:
  explicit TabsFromOtherDevicesSidePanelUI(content::WebUI* web_ui);
  TabsFromOtherDevicesSidePanelUI(const TabsFromOtherDevicesSidePanelUI&) =
      delete;
  TabsFromOtherDevicesSidePanelUI& operator=(
      const TabsFromOtherDevicesSidePanelUI&) = delete;
  ~TabsFromOtherDevicesSidePanelUI() override;

  // Expected to be called immediately after construction.
  void SetBrowserWindowInterface(
      BrowserWindowInterface* browser_window_interface);

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler>
          pending_page_handler);

  BrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_;
  }

  void SetMetricsRecorder(
      base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics> metrics_recorder) {
    metrics_recorder_ = metrics_recorder;
  }
  base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics> metrics_recorder() {
    return metrics_recorder_;
  }

  static constexpr std::string_view GetWebUIName() {
    return "TabsFromOtherDevicesSidePanel";
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  std::unique_ptr<browser_sync::ForeignSessionHandler> foreign_session_handler_;

  base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics> metrics_recorder_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_UI_H_
