// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom-data-view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event_constants.h"

namespace {
// Measurement marks.
constexpr char kChangeVisibleModeToReloadStartMark[] =
    "ReloadButton.ChangeVisibleModeToReload.Start";
constexpr char kChangeVisibleModeToStopStartMark[] =
    "ReloadButton.ChangeVisibleModeToStop.Start";
constexpr char kInputMouseReleaseStartMark[] =
    "ReloadButton.Input.MouseRelease.Start";

// Histogram names.
constexpr char kInputToReloadMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToReload.MouseRelease";
constexpr char kInputToStopMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToStop.MouseRelease";

int ToUIEventFlags(
    const std::vector<reload_button::mojom::ClickDispositionFlag>& flags) {
  using reload_button::mojom::ClickDispositionFlag;
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
    }
  }
  return event_flags;
}

}  // namespace

ReloadButtonPageHandler::ReloadButtonPageHandler(
    mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver,
    mojo::PendingRemote<reload_button::mojom::Page> page,
    content::WebContents* web_contents,
    CommandUpdater* command_updater)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(web_contents),
      command_updater_(command_updater) {
  CHECK(web_contents_);
  CHECK(command_updater_);
}

ReloadButtonPageHandler::~ReloadButtonPageHandler() = default;

MetricsReporter* ReloadButtonPageHandler::GetMetricsReporter() {
  MetricsReporterService* service =
      MetricsReporterService::GetFromWebContents(web_contents_);
  return service ? service->metrics_reporter() : nullptr;
}

void ReloadButtonPageHandler::Reload(
    bool ignore_cache,
    const std::vector<reload_button::mojom::ClickDispositionFlag>& flags) {
  command_updater_->ExecuteCommandWithDisposition(
      ignore_cache ? IDC_RELOAD_BYPASSING_CACHE : IDC_RELOAD,
      ui::DispositionFromEventFlags(ToUIEventFlags(flags)));

  // Gets the current time immediately after executing the command.
  const base::TimeTicks now = base::TimeTicks::Now();

  auto* metrics_reporter = GetMetricsReporter();
  if (!metrics_reporter) {
    return;
  }

  // MouseRelease
  metrics_reporter->Measure(
      kInputMouseReleaseStartMark, now,
      base::BindOnce(&ReloadButtonPageHandler::OnMeasureResultAndClearMark,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputToReloadMouseReleaseHistogram,
                     kInputMouseReleaseStartMark));
  // TODO(crbug.com/448794588): Handle KeyPress events.
}

void ReloadButtonPageHandler::StopReload() {
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
      base::BindOnce(&ReloadButtonPageHandler::OnMeasureResultAndClearMark,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputToStopMouseReleaseHistogram,
                     kInputMouseReleaseStartMark));
  // TODO(crbug.com/448794588): Handle KeyPress events.
}

void ReloadButtonPageHandler::ShowContextMenu(int32_t offset_x,
                                              int32_t offset_y) {
  content::ContextMenuParams params;
  params.x = offset_x;
  params.y = offset_y;
  web_contents_->GetDelegate()->HandleContextMenu(
      *web_contents_->GetPrimaryMainFrame(), params);
}

void ReloadButtonPageHandler::SetReloadButtonState(bool is_loading,
                                                   bool is_menu_enabled) {
  if (auto* metrics_reporter = GetMetricsReporter()) {
    auto* mark = is_loading ? kChangeVisibleModeToStopStartMark
                            : kChangeVisibleModeToReloadStartMark;
    metrics_reporter->Mark(mark);
  }

  if (page_) {
    page_->SetReloadButtonState(is_loading, is_menu_enabled);
  }
}

void ReloadButtonPageHandler::OnMeasureResultAndClearMark(
    const std::string& histogram_name,
    const std::string& start_mark,
    base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(histogram_name, duration, base::Milliseconds(1),
                                base::Minutes(3), 100);
  if (auto* metrics_reporter = GetMetricsReporter()) {
    metrics_reporter->ClearMark(start_mark);
  }
}
