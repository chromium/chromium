// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"

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
  };

  BrowserControlsService(
      mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsService>
          service,
      mojo::PendingRemote<browser_controls_api::mojom::BrowserControlsObserver>
          observer,
      content::WebContents* web_contents,
      CommandUpdater* command_updater,
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

  // browser_controls_api::mojom::BrowserControlsService:
  void ReloadFromClick(
      bool bypass_cache,
      const std::vector<browser_controls_api::mojom::ClickDispositionFlag>&
          click_flags) override;
  void StopLoad() override;
  void ShowContextMenu(browser_controls_api::mojom::ContextMenuType menu_type,
                       const gfx::Point& viewport_coordinate_css_pixels,
                       ui::mojom::MenuSourceType source) override;
  void OnPageInitialized() override;

 private:
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
  // Not owned.
  const raw_ptr<CommandUpdater> command_updater_;

  raw_ptr<BrowserControlsServiceDelegate> delegate_;

  // Must be the last member.
  base::WeakPtrFactory<BrowserControlsService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
