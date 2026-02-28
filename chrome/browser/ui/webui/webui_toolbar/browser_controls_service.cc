// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event_constants.h"

namespace {
// Measurement marks.
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

}  // namespace

namespace browser_controls_api {

BrowserControlsService::BrowserControlsService(
    mojo::PendingReceiver<mojom::BrowserControlsService> service,
    std::unique_ptr<BrowserControlsAdapter> browser_adapter,
    MetricsReporter* metrics_reporter,
    BrowserControlsServiceDelegate* delegate)
    : service_(this, std::move(service)),
      browser_adapter_(std::move(browser_adapter)),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate) {
  CHECK(browser_adapter_);
}

BrowserControlsService::~BrowserControlsService() = default;

void BrowserControlsService::ReloadFromClick(
    bool bypass_cache,
    const std::vector<browser_controls_api::mojom::ClickDispositionFlag>&
        click_flags) {
  // This is called in order to signal that external protocol dialogs are
  // allowed to show due to a user action, which are likely to happen on the
  // next page load after the reload button is clicked.
  // Ideally, the browser UI's event system would notify ExternalProtocolHandler
  // that a user action occurred and we are OK to open the dialog, but for some
  // reason that isn't happening every time the reload button is clicked. See
  // http://crbug.com/1206456
  if (delegate_) {
    delegate_->PermitLaunchUrl();
  }

  browser_adapter_->Reload(
      bypass_cache, ui::DispositionFromEventFlags(ToUIEventFlags(click_flags)));

  // Gets the current time immediately after executing the command.
  const base::TimeTicks now = base::TimeTicks::Now();
  // MouseRelease
  metrics_reporter_->Measure(
      kInputMouseReleaseStartMark, now,
      base::BindOnce(&BrowserControlsService::OnMeasureResultAndClearMark,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputToReloadMouseReleaseHistogram,
                     kInputMouseReleaseStartMark));
  // TODO(crbug.com/448794588): Handle KeyPress events.
}

void BrowserControlsService::StopLoad() {
  browser_adapter_->Stop();

  // Gets the current time immediately after executing the command.
  const base::TimeTicks now = base::TimeTicks::Now();
  // MouseRelease
  metrics_reporter_->Measure(
      kInputMouseReleaseStartMark, now,
      base::BindOnce(&BrowserControlsService::OnMeasureResultAndClearMark,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputToStopMouseReleaseHistogram,
                     kInputMouseReleaseStartMark));
  // TODO(crbug.com/448794588): Handle KeyPress events.
}

void BrowserControlsService::SplitActiveTab() {
  // We only reach here if the frontend decided we need to CREATE a split.
  // We don't need to check IsActiveTabInSplit() or handle the menu here.
  browser_adapter_->CreateNewSplitTab();
}

void BrowserControlsService::SetDelegate(
    BrowserControlsServiceDelegate* delegate) {
  delegate_ = delegate;
}

void BrowserControlsService::OnMeasureResultAndClearMark(
    const std::string& histogram_name,
    const std::string& start_mark,
    base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(histogram_name, duration, base::Milliseconds(1),
                                base::Minutes(3), 100);
  metrics_reporter_->ClearMark(start_mark);
}

}  // namespace browser_controls_api
