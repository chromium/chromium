// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TOOLBAR_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TOOLBAR_UI_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/base/pointer/touch_ui_controller.h"

class MetricsReporter;

namespace toolbar_ui_api {

class ToolbarUIService : public toolbar_ui_api::mojom::ToolbarUIService {
 public:
  class ToolbarUIServiceDelegate {
   public:
    virtual ~ToolbarUIServiceDelegate() = default;
    virtual void HandleContextMenu(
        toolbar_ui_api::mojom::ContextMenuType menu_type,
        gfx::Point viewport_coordinate_css_pixels,
        ui::mojom::MenuSourceType source) = 0;
    virtual void OnPageInitialized() = 0;
  };

  ToolbarUIService(
      mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService> service,
      std::unique_ptr<NavigationControlsStateFetcher> state_fetcher,
      MetricsReporter* metrics_reporter,
      ToolbarUIServiceDelegate* delegate);

  ToolbarUIService(const ToolbarUIService&) = delete;
  ToolbarUIService& operator=(const ToolbarUIService&) = delete;

  ~ToolbarUIService() override;

  void SetDelegate(ToolbarUIServiceDelegate* delegate);

  void OnNavigationControlsStateChanged(
      const mojom::NavigationControlsStatePtr& state);

  // toolbar_ui_api::mojom::ToolbarUIService:
  void Bind(BindCallback callback) override;
  void ShowContextMenu(toolbar_ui_api::mojom::ContextMenuType menu_type,
                       const gfx::Point& viewport_coordinate_css_pixels,
                       ui::mojom::MenuSourceType source) override;
  void OnPageInitialized() override;

 private:
  mojo::Receiver<toolbar_ui_api::mojom::ToolbarUIService> service_;
  mojo::RemoteSet<toolbar_ui_api::mojom::ToolbarUIObserver> observers_;

  std::unique_ptr<NavigationControlsStateFetcher> state_fetcher_;

  // Not owned.
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<ToolbarUIServiceDelegate> delegate_;

  // Must be the last member.
  base::WeakPtrFactory<ToolbarUIService> weak_ptr_factory_{this};
};

}  // namespace toolbar_ui_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TOOLBAR_UI_SERVICE_H_
