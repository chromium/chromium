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
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace {
// Measurement marks.
const char kChangeVisibleModeToReloadStartMark[] =
    "ReloadButton.ChangeVisibleModeToReload.Start";
const char kChangeVisibleModeToStopStartMark[] =
    "ReloadButton.ChangeVisibleModeToStop.Start";
constexpr char kInputMouseReleaseStartMark[] =
    "ReloadButton.Input.MouseRelease.Start";
constexpr char kReloadForMouseReleaseEndMark[] =
    "ReloadButton.Reload.MouseRelease.End";
constexpr char kStopForMouseReleaseEndMark[] =
    "ReloadButton.Stop.MouseRelease.End";

// Histogram names.
constexpr char kInputToReloadMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToReload.MouseRelease";
constexpr char kInputToStopMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToStop.MouseRelease";
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

void ReloadButtonPageHandler::SetMetricsReporterForTesting(
    MetricsReporter* metrics_reporter) {
  metrics_reporter_for_testing_ = metrics_reporter;
}

MetricsReporter* ReloadButtonPageHandler::GetMetricsReporter() {
  if (metrics_reporter_for_testing_) {  // IN-TEST
    return metrics_reporter_for_testing_;
  }

  auto* service = MetricsReporterService::GetFromWebContents(web_contents_);
  return service ? service->metrics_reporter() : nullptr;
}

void ReloadButtonPageHandler::Reload(bool ignore_cache) {
  command_updater_->ExecuteCommand(ignore_cache ? IDC_RELOAD_BYPASSING_CACHE
                                                : IDC_RELOAD);

  if (auto* metrics_reporter = GetMetricsReporter()) {
    metrics_reporter->Mark(kReloadForMouseReleaseEndMark);
    MaybeRecordInputToReloadMetric(metrics_reporter);
  }
}

void ReloadButtonPageHandler::StopReload() {
  command_updater_->ExecuteCommand(IDC_STOP);

  // TODO(crbug.com/448794588): Handle key press metric marks.
  if (auto* metrics_reporter = GetMetricsReporter()) {
    metrics_reporter->Mark(kStopForMouseReleaseEndMark);
    MaybeRecordInputToStopMetric(metrics_reporter);
  }
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

void ReloadButtonPageHandler::MaybeRecordInputToReloadMetric(
    MetricsReporter* metrics_reporter) {
  metrics_reporter->HasMark(
      kInputMouseReleaseStartMark,
      base::BindOnce(&ReloadButtonPageHandler::OnHasStartMarkResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputMouseReleaseStartMark, kReloadForMouseReleaseEndMark,
                     kInputToReloadMouseReleaseHistogram));
  // TODO(crbug.com/448794588): Handle key press metrics.
}

void ReloadButtonPageHandler::MaybeRecordInputToStopMetric(
    MetricsReporter* metrics_reporter) {
  metrics_reporter->HasMark(
      kInputMouseReleaseStartMark,
      base::BindOnce(&ReloadButtonPageHandler::OnHasStartMarkResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     kInputMouseReleaseStartMark, kStopForMouseReleaseEndMark,
                     kInputToStopMouseReleaseHistogram));

  // TODO(crbug.com/448794588): Handle key press metrics.
}

void ReloadButtonPageHandler::OnHasStartMarkResult(
    const std::string& start_mark,
    const std::string& end_mark,
    const std::string& histogram_name,
    bool has_start_mark) {
  auto* metrics_reporter = GetMetricsReporter();
  if (!metrics_reporter) {
    return;
  }

  if (!has_start_mark) {
    metrics_reporter->ClearMark(end_mark);
    return;
  }
  metrics_reporter->Measure(
      start_mark, end_mark,
      base::BindOnce(&ReloadButtonPageHandler::OnMeasureResult,
                     weak_ptr_factory_.GetWeakPtr(), histogram_name));
  metrics_reporter->ClearMark(start_mark);
  metrics_reporter->ClearMark(end_mark);
}

void ReloadButtonPageHandler::OnMeasureResult(const std::string& histogram_name,
                                              base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(histogram_name, duration, base::Milliseconds(1),
                                base::Minutes(3), 100);
}
