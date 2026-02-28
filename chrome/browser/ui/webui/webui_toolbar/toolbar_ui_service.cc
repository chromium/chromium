// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/toolbar_ui_service.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"

namespace {
// Measurement marks.
constexpr char kChangeVisibleModeToLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToLoading.Start";
constexpr char kChangeVisibleModeToNotLoadingStartMark[] =
    "ToolbarUI.ChangeVisibleModeToNotLoading.Start";
}  // namespace

namespace toolbar_ui_api {

ToolbarUIService::ToolbarUIService(
    mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService> service,
    std::unique_ptr<NavigationControlsStateFetcher> state_fetcher,
    MetricsReporter* metrics_reporter,
    ToolbarUIServiceDelegate* delegate)
    : service_(this, std::move(service)),
      state_fetcher_(std::move(state_fetcher)),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate) {
  CHECK(state_fetcher_);
}

ToolbarUIService::~ToolbarUIService() = default;

void ToolbarUIService::SetDelegate(ToolbarUIServiceDelegate* delegate) {
  delegate_ = delegate;
}

void ToolbarUIService::OnNavigationControlsStateChanged(
    const mojom::NavigationControlsStatePtr& state) {
  auto* mark = state->reload_control_state->is_navigation_loading
                   ? kChangeVisibleModeToLoadingStartMark
                   : kChangeVisibleModeToNotLoadingStartMark;
  metrics_reporter_->Mark(mark);

  for (auto& observer : observers_) {
    observer->OnNavigationControlsStateChanged(state.Clone());
  }
}

void ToolbarUIService::Bind(BindCallback callback) {
  auto result = toolbar_ui_api::mojom::InitialState::New();
  result->state = state_fetcher_->GetNavigationControlsState();

  mojo::Remote<toolbar_ui_api::mojom::ToolbarUIObserver> observer;
  result->update_stream = observer.BindNewPipeAndPassReceiver();

  observers_.Add(std::move(observer));

  std::move(callback).Run(std::move(result));
}

void ToolbarUIService::ShowContextMenu(
    toolbar_ui_api::mojom::ContextMenuType menu_type,
    const gfx::Point& viewport_coordinate_css_pixels,
    ui::mojom::MenuSourceType source) {
  if (delegate_) {
    delegate_->HandleContextMenu(menu_type, viewport_coordinate_css_pixels,
                                 source);
  }
}

void ToolbarUIService::OnPageInitialized() {
  if (delegate_) {
    delegate_->OnPageInitialized();
  }
}

}  // namespace toolbar_ui_api
