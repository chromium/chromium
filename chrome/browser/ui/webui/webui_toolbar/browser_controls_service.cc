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
#include "base/types/expected_macros.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "mojo/public/mojom/base/error.mojom.h"
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

using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

}  // namespace

namespace browser_controls_api {

// static
base::expected<ui::EventFlags, mojo_base::mojom::ErrorPtr>
BrowserControlsService::ToUiEventFlags(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        flags) {
  using browser_controls_api::mojom::EventDispositionFlag;
  ui::EventFlags event_flags = 0;
  for (auto& flag : flags) {
    switch (flag) {
      case EventDispositionFlag::kMiddleMouseButton: {
        event_flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      }
      case EventDispositionFlag::kAltKeyDown: {
        event_flags |= ui::EF_ALT_DOWN;
        break;
      }
      case EventDispositionFlag::kMetaKeyDown: {
        event_flags |= ui::EF_COMMAND_DOWN;
        break;
      }
      case EventDispositionFlag::kShiftKeyDown: {
        event_flags |= ui::EF_SHIFT_DOWN;
        break;
      }
      case EventDispositionFlag::kControlKeyDown: {
        event_flags |= ui::EF_CONTROL_DOWN;
        break;
      }
      case EventDispositionFlag::kAltGrKeyDown: {
        event_flags |= ui::EF_ALTGR_DOWN;
        break;
      }
      case EventDispositionFlag::kUnspecified:
        return base::unexpected(Error::New(
            Code::kInvalidArgument, "invalid event disposition flag received"));
    }
  }
  return event_flags;
}

BrowserControlsService::BrowserControlsService(
    mojo::PendingReceiver<mojom::BrowserControlsService> service,
    std::unique_ptr<BrowserControlsAdapter> browser_adapter,
    MetricsReporter* metrics_reporter,
    BrowserControlsServiceDelegate* delegate)
    : service_(&bridge_, std::move(service)),
      browser_adapter_(std::move(browser_adapter)),
      metrics_reporter_(metrics_reporter),
      delegate_(delegate) {
  CHECK(browser_adapter_);
}

BrowserControlsService::~BrowserControlsService() = default;

BrowserControlsService::ReloadFromClickResult
BrowserControlsService::ReloadFromClick(
    bool bypass_cache,
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        click_flags) {
  // This is called in order to signal that external protocol dialogs are
  // allowed to show due to a user action, which are likely to happen on the
  // next page load after the reload button is clicked.
  // Ideally, the browser UI's event system would notify ExternalProtocolHandler
  // that a user action occurred and we are OK to open the dialog, but for some
  // reason that isn't happening every time the reload button is clicked. See
  // http://crbug.com/40180927
  if (delegate_) {
    delegate_->PermitLaunchUrl();
  }

  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(click_flags));

  browser_adapter_->Reload(bypass_cache,
                           ui::DispositionFromEventFlags(converted));

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
  return std::monostate();
}

BrowserControlsService::StopLoadResult BrowserControlsService::StopLoad() {
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
  return std::monostate();
}

BrowserControlsService::BackResult BrowserControlsService::Back(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        flags) {
  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(flags));
  browser_adapter_->Back(ui::DispositionFromEventFlags(converted));
  return std::monostate();
}

BrowserControlsService::ForwardResult BrowserControlsService::Forward(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        flags) {
  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(flags));
  browser_adapter_->Forward(ui::DispositionFromEventFlags(converted));
  return std::monostate();
}

BrowserControlsService::BackButtonHoveredResult
BrowserControlsService::BackButtonHovered() {
  browser_adapter_->BackButtonHovered();

  return std::monostate();
}

BrowserControlsService::SplitActiveTabResult
BrowserControlsService::SplitActiveTab() {
  // TODO: need to check IsActiveTabInSplit() first.
  browser_adapter_->CreateNewSplitTab();
  return std::monostate();
}

BrowserControlsService::NavigateHomeResult BrowserControlsService::NavigateHome(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        click_flags) {
  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(click_flags));
  browser_adapter_->NavigateHome(ui::DispositionFromEventFlags(converted));
  return std::monostate();
}

BrowserControlsService::NavigateResult BrowserControlsService::Navigate(
    const GURL& url) {
  browser_adapter_->Navigate(url);
  return std::monostate();
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
