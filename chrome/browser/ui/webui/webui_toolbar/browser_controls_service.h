// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/views/controls/menu/menu_runner.h"

class MetricsReporter;

namespace browser_controls_api {

class BrowserControlsService
    : public browser_controls_api::mojom::BrowserControlsService {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void HandleContextMenu(
        browser_controls_api::mojom::ContextMenuType menu_type,
        gfx::Point viewport_coordinate_css_pixels,
        ui::mojom::MenuSourceType source) = 0;
    virtual void OnPageInitialized() = 0;
    virtual void PermitLaunchUrl() = 0;
  };

  BrowserControlsService(
      mojo::PendingReceiver<mojom::BrowserControlsService> service,
      std::unique_ptr<BrowserControlsAdapter> browser_adapter,
      std::unique_ptr<NavigationControlsStateFetcher> state_fetcher,
      MetricsReporter* metrics_reporter,
      Delegate* delegate);

  BrowserControlsService(const BrowserControlsService&) = delete;
  BrowserControlsService& operator=(const BrowserControlsService&) = delete;

  ~BrowserControlsService() override;

  void SetDelegate(Delegate* delegate);

  // browser_controls_api::mojom::BrowserControlsService:
  void Bind(BindCallback callback) override;
  void ReloadFromClick(
      bool bypass_cache,
      const std::vector<mojom::ClickDispositionFlag>& click_flags) override;
  void StopLoad() override;
  void ShowContextMenu(mojom::ContextMenuType menu_type,
                       const gfx::Point& viewport_coordinate_css_pixels,
                       ui::mojom::MenuSourceType source) override;
  void OnPageInitialized() override;
  void SplitActiveTab() override;

  void OnNavigationControlsStateChanged(
      const browser_controls_api::mojom::NavigationControlsStatePtr& state);

 private:
  // Callback for `MetricsReporter::Measure()`. Records the resulting
  // base::TimeDelta to the given UMA histogram and clears the start mark.
  void OnMeasureResultAndClearMark(const std::string& histogram_name,
                                   const std::string& start_mark,
                                   base::TimeDelta duration);

  mojo::Receiver<browser_controls_api::mojom::BrowserControlsService> service_;
  mojo::RemoteSet<browser_controls_api::mojom::BrowserControlsObserver>
      observers_;

  std::unique_ptr<BrowserControlsAdapter> browser_adapter_;
  std::unique_ptr<NavigationControlsStateFetcher> state_fetcher_;

  // Not owned.
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<Delegate> delegate_;

  // Must be the last member.
  base::WeakPtrFactory<BrowserControlsService> weak_ptr_factory_{this};
};

}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_BROWSER_CONTROLS_SERVICE_H_
