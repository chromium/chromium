// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/split_tabs_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {
// Measurement marks.
constexpr char kChangeVisibleModeToLoadingStartMark[] =
    "BrowserControls.ChangeVisibleModeToLoading.Start";
constexpr char kChangeVisibleModeToNotLoadingStartMark[] =
    "BrowserControls.ChangeVisibleModeToNotLoading.Start";
constexpr char kInputMouseReleaseStartMark[] =
    "ReloadButton.Input.MouseRelease.Start";

// Histogram names.
constexpr char kInputToReloadMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToReload.MouseRelease";
constexpr char kInputToStopMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToStop.MouseRelease";

int ToUIEventFlags(
    const std::vector<browser_controls_api::mojom::ClickDispositionFlag>&
        flags) {
  using browser_controls_api::mojom::ClickDispositionFlag;
  int event_flags = 0;
  for (auto& flag : flags) {
    switch (flag) {
      case ClickDispositionFlag::kMiddleMouseButton: {
        event_flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      }
      case ClickDispositionFlag::kAltKeyDown: {
        event_flags |= ui::EF_ALT_DOWN;
        break;
      }
      case ClickDispositionFlag::kMetaKeyDown: {
        event_flags |= ui::EF_COMMAND_DOWN;
        break;
      }
      case ClickDispositionFlag::kUnspecified:
        NOTREACHED() << "Unexpected ClickDispositionFlag::kUnspecified.";
    }
  }
  return event_flags;
}

browser_controls_api::mojom::LayoutConstantsPtr GetLayoutConstantsStruct() {
  return browser_controls_api::mojom::LayoutConstants::New(
      GetLayoutConstant(LayoutConstant::kToolbarButtonHeight),
      GetLayoutConstant(LayoutConstant::kToolbarButtonIconSize));
}

}  // namespace

BrowserControlsService::BrowserControlsService(
    mojo::PendingReceiver<browser_controls_api::mojom::BrowserControlsService>
        service,
    content::WebContents* web_contents,
    CommandUpdater* command_updater,
    BrowserWindowInterface* browser,
    BrowserControlsServiceDelegate* delegate)
    : service_(this, std::move(service)),
      web_contents_(web_contents),
      command_updater_(command_updater),
      browser_(browser),
      delegate_(delegate) {
  CHECK(web_contents_);
  CHECK(command_updater_);

  touch_ui_subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(base::BindRepeating(
          &BrowserControlsService::OnTouchUiChanged, base::Unretained(this)));
}

BrowserControlsService::~BrowserControlsService() = default;

MetricsReporter* BrowserControlsService::GetMetricsReporter() {
  MetricsReporterService* service =
      MetricsReporterService::GetFromWebContents(web_contents_);
  return service ? service->metrics_reporter() : nullptr;
}

void BrowserControlsService::AddObserver(
    mojo::PendingRemote<browser_controls_api::mojom::BrowserControlsObserver>
        observer) {
  if (observer_.is_bound()) {
    observer_.reset();
  }
  observer_.Bind(std::move(observer));
}

void BrowserControlsService::ReloadFromClick(
    bool bypass_cache,
    const std::vector<browser_controls_api::mojom::ClickDispositionFlag>&
        click_flags) {
  command_updater_->ExecuteCommandWithDisposition(
      bypass_cache ? IDC_RELOAD_BYPASSING_CACHE : IDC_RELOAD,
      ui::DispositionFromEventFlags(ToUIEventFlags(click_flags)));

  // Gets the current time immediately after executing the command.
  const base::TimeTicks now = base::TimeTicks::Now();

  auto* metrics_reporter = GetMetricsReporter();
  if (!metrics_reporter) {
    return;
  }

  // MouseRelease
  metrics_reporter->Measure(
      kInputMouseReleaseStartMark, now,
      base::BindOnce(&BrowserControlsService::OnMeasureResultAndClearMark,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputToReloadMouseReleaseHistogram,
                     kInputMouseReleaseStartMark));
  // TODO(crbug.com/448794588): Handle KeyPress events.
}

void BrowserControlsService::StopLoad() {
  command_updater_->ExecuteCommandWithDisposition(
      IDC_STOP, WindowOpenDisposition::CURRENT_TAB);
  // Gets the current time immediately after executing the command.
  const base::TimeTicks now = base::TimeTicks::Now();

  auto* metrics_reporter = GetMetricsReporter();
  if (!metrics_reporter) {
    return;
  }

  // MouseRelease
  metrics_reporter->Measure(
      kInputMouseReleaseStartMark, now,
      base::BindOnce(&BrowserControlsService::OnMeasureResultAndClearMark,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputToStopMouseReleaseHistogram,
                     kInputMouseReleaseStartMark));
  // TODO(crbug.com/448794588): Handle KeyPress events.
}

void BrowserControlsService::ShowContextMenu(
    browser_controls_api::mojom::ContextMenuType menu_type,
    const gfx::Point& viewport_coordinate_css_pixels,
    ui::mojom::MenuSourceType source) {
  if (delegate_) {
    delegate_->HandleContextMenu(menu_type, viewport_coordinate_css_pixels,
                                 source);
  }
}

void BrowserControlsService::OnPageInitialized() {
  if (delegate_) {
    delegate_->OnPageInitialized();
  }
}

void BrowserControlsService::OnDevToolsStatusChanged(
    browser_controls_api::mojom::DevToolsState state) {
  if (observer_) {
    observer_->OnDevToolsStatusChanged(state);
  }
}

void BrowserControlsService::OnNavigationStatusChanged(
    browser_controls_api::mojom::NavigationState state) {
  if (auto* metrics_reporter = GetMetricsReporter()) {
    auto* mark = state == browser_controls_api::mojom::NavigationState::kLoading
                     ? kChangeVisibleModeToLoadingStartMark
                     : kChangeVisibleModeToNotLoadingStartMark;
    metrics_reporter->Mark(mark);
  }

  if (observer_) {
    observer_->OnNavigationStatusChanged(state);
  }
}

void BrowserControlsService::OnContextMenuStateChanged(
    browser_controls_api::mojom::ContextMenuType menu_type,
    browser_controls_api::mojom::ContextMenuState state) {
  if (observer_) {
    observer_->OnContextMenuStateChanged(menu_type, state);
  }
}

void BrowserControlsService::SplitActiveTab() {
  // We only reach here if the frontend decided we need to CREATE a split.
  // We don't need to check IsActiveTabInSplit() or handle the menu here.
  chrome::NewSplitTab(browser_,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
}

void BrowserControlsService::GetTabSplitState(
    GetTabSplitStateCallback callback) {
  webui_toolbar::TabSplitStatus status =
      webui_toolbar::ComputeTabSplitStatus(browser_);
  std::move(callback).Run(status.is_split, status.location);
}

void BrowserControlsService::GetButtonPinState(
    browser_controls_api::mojom::ToolbarButtonType type,
    GetButtonPinStateCallback callback) {
  bool is_pinned = webui_toolbar::IsButtonPinned(browser_, type);
  std::move(callback).Run(is_pinned);
}

void BrowserControlsService::GetLayoutConstants(
    GetLayoutConstantsCallback callback) {
  std::move(callback).Run(GetLayoutConstantsStruct());
}

void BrowserControlsService::OnTabSplitStatusChanged(
    bool is_split,
    browser_controls_api::mojom::SplitTabActiveLocation location) {
  if (observer_) {
    observer_->OnTabSplitStatusChanged(is_split, location);
  }
}

void BrowserControlsService::OnButtonPinStateChanged(
    browser_controls_api::mojom::ToolbarButtonType type,
    bool is_pinned) {
  if (observer_) {
    observer_->OnButtonPinStateChanged(type, is_pinned);
  }
}

void BrowserControlsService::OnTouchUiChanged() {
  if (observer_) {
    observer_->OnLayoutChanged(GetLayoutConstantsStruct());
  }
}

void BrowserControlsService::OnMeasureResultAndClearMark(
    const std::string& histogram_name,
    const std::string& start_mark,
    base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(histogram_name, duration, base::Milliseconds(1),
                                base::Minutes(3), 100);
  if (auto* metrics_reporter = GetMetricsReporter()) {
    metrics_reporter->ClearMark(start_mark);
  }
}
