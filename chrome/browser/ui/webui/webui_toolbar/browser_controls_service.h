// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/controls/menu/menu_runner.h"

class BrowserWindowInterface;
class CommandUpdater;
class MetricsReporter;

namespace content {
class WebContents;
}  // namespace content

class BrowserControlsService
    : public browser_controls_api::mojom::BrowserControlsService {
 public:
  class BrowserControlsServiceDelegate {
   public:
    virtual void HandleContextMenu(
        browser_controls_api::mojom::ContextMenuType menu_type,
        gfx::Point viewport_coordinate_css_pixels,
        ui::mojom::MenuSourceType source) = 0;
    virtual void OnPageInitialized() = 0;
    virtual void PermitLaunchUrl() = 0;
  };

  BrowserControlsService(
      mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsService>
          service,
      content::WebContents* web_contents,
      CommandUpdater* command_updater,
      BrowserWindowInterface* browser,
      BrowserControlsServiceDelegate* delegate);

  BrowserControlsService(const BrowserControlsService&) = delete;
  BrowserControlsService& operator=(const BrowserControlsService&) = delete;

  ~BrowserControlsService() override;

  void OnDevToolsStatusChanged(
      browser_controls_api::mojom::DevToolsState state);
  void OnNavigationStatusChanged(
      browser_controls_api::mojom::NavigationState state);
  void OnContextMenuStateChanged(
      browser_controls_api::mojom::ContextMenuType menu_type,
      browser_controls_api::mojom::ContextMenuState state);

  void SetDelegate(BrowserControlsServiceDelegate* delegate);

  // browser_controls_api::mojom::BrowserControlsService:
  void AddObserver(
      mojo::PendingRemote<browser_controls_api::mojom::BrowserControlsObserver>
          observer) override;
  void ReloadFromClick(
      bool bypass_cache,
      const std::vector<browser_controls_api::mojom::ClickDispositionFlag>&
          click_flags) override;
  void StopLoad() override;
  void ShowContextMenu(browser_controls_api::mojom::ContextMenuType menu_type,
                       const gfx::Point& viewport_coordinate_css_pixels,
                       ui::mojom::MenuSourceType source) override;
  void OnPageInitialized() override;
  void SplitActiveTab() override;
  void GetTabSplitState(GetTabSplitStateCallback callback) override;
  void GetButtonPinState(browser_controls_api::mojom::ToolbarButtonType type,
                         GetButtonPinStateCallback callback) override;
  void GetLayoutConstants(GetLayoutConstantsCallback callback) override;

  // Updates the split status of the active tab in the renderer.
  void OnTabSplitStatusChanged(
      bool is_split,
      browser_controls_api::mojom::SplitTabActiveLocation location);

  // Updates the pin state of the specified button in the renderer.
  void OnButtonPinStateChanged(
      browser_controls_api::mojom::ToolbarButtonType type,
      bool is_pinned);

 private:
  void OnTouchUiChanged();

  // Returns the MetricsReporter associated with `web_contents_` or nullptr.
  //
  // This method fetches the reporter from the MetricsReporterService associated
  // with `web_contents_` each time it is called. This is necessary because the
  // MetricsReporterService lifetime is tied to `web_contents_`, which can be
  // destroyed earlier than this BrowserControlsService.
  MetricsReporter* GetMetricsReporter();

  // Callback for `MetricsReporter::Measure()`. Records the resulting
  // base::TimeDelta to the given UMA histogram and clears the start mark.
  void OnMeasureResultAndClearMark(const std::string& histogram_name,
                                   const std::string& start_mark,
                                   base::TimeDelta duration);

  mojo::Receiver<browser_controls_api::mojom::BrowserControlsService> service_;
  mojo::Remote<browser_controls_api::mojom::BrowserControlsObserver> observer_;

  // Not owned.
  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<CommandUpdater> command_updater_;
  const raw_ptr<BrowserWindowInterface> browser_;

  raw_ptr<BrowserControlsServiceDelegate> delegate_;

  base::CallbackListSubscription touch_ui_subscription_;

  // Must be the last member.
  base::WeakPtrFactory<BrowserControlsService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
