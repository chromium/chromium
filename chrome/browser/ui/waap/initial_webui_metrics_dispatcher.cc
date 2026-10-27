// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_metrics_dispatcher.h"

#include "base/check.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"

// static
InitialWebUIWebContentsMetricsDispatcher*
InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  InitialWebUIWebContentsMetricsDispatcher::CreateForWebContents(web_contents);
  return content::WebContentsUserData<
      InitialWebUIWebContentsMetricsDispatcher>::FromWebContents(web_contents);
}

InitialWebUIMetricsDispatcher::InitialWebUIMetricsDispatcher() = default;

InitialWebUIMetricsDispatcher::~InitialWebUIMetricsDispatcher() = default;

void InitialWebUIMetricsDispatcher::OnCreated() {
  // Like the paint metrics, only record creation for windows that track
  // InitialWebUI metrics (e.g. not popups, which have no manager).
  if (!GetWindowMetricsManager()) {
    return;
  }
  WaapUIMetricsService* service = GetService();
  CHECK(service);
  service->OnReloadButtonCreated();
}

void InitialWebUIMetricsDispatcher::OnCommit(base::TimeTicks init_time,
                                             base::TimeTicks launched_time) {
  // Unlike paints, a commit can happen long before any window takes the
  // WebContents (e.g. profile prewarming), or without one ever doing so.
  // Buffer the process times until a manager is available so the startup
  // metric is only attributed to a WebContents that a window actually uses.
  // TODO(crbug.com/548363252): For prewarmed WebContents that commit before the
  // window is shown, `SetBrowserWindowInterface()` flushes these metrics during
  // window initialization before `OnBrowserWindowShowRequested()` checks
  // `IsMinimized()`, so minimized windows are not yet skipped in that case.
  renderer_process_times_ = RendererProcessTimes{
      .init_time = init_time, .launched_time = launched_time};
  FlushMetricsToManager();
}

void InitialWebUIMetricsDispatcher::OnFirstPaint(base::TimeTicks paint_time) {
  if (paint_time.is_null() || is_first_paint_handled_) {
    return;
  }
  is_first_paint_handled_ = true;

  // In production, a WebContents only paints after it is attached to a
  // visible window. Require `manager` so that non-normal windows (e.g. popups,
  // which have no manager), minimized windows, and `SkipStartupForTesting()`
  // are all respected.
  InitialWebUIWindowMetricsManager* manager = GetWindowMetricsManager();
  if (!manager || manager->should_skip_latency_metrics()) {
    return;
  }

  bool is_startup = false;
  if (!manager->should_skip_startup_metrics()) {
    WaapUIMetricsService* service = GetService();
    CHECK(service);
    if (!WaapUIMetricsService::IsStartupFirstPaintRecorded()) {
      is_startup = true;
      service->OnFirstPaint(paint_time);
    }
  }

  manager->OnReloadButtonFirstPaint(paint_time, is_startup);
}

void InitialWebUIMetricsDispatcher::OnFirstContentfulPaint(
    base::TimeTicks fcp_time) {
  if (fcp_time.is_null() || is_first_contentful_paint_handled_) {
    return;
  }
  is_first_contentful_paint_handled_ = true;

  InitialWebUIWindowMetricsManager* manager = GetWindowMetricsManager();
  if (!manager || manager->should_skip_latency_metrics()) {
    return;
  }

  bool is_startup = false;
  if (!manager->should_skip_startup_metrics()) {
    WaapUIMetricsService* service = GetService();
    CHECK(service);
    if (!WaapUIMetricsService::IsStartupFirstContentfulPaintRecorded()) {
      is_startup = true;
      service->OnFirstContentfulPaint(fcp_time);
    }
  }

  manager->OnReloadButtonFirstContentfulPaint(fcp_time, is_startup);
}

void InitialWebUIMetricsDispatcher::FlushMetricsToManager() {
  InitialWebUIWindowMetricsManager* manager = GetWindowMetricsManager();
  if (!manager) {
    OnManagerUnavailable();
    return;
  }

  if (renderer_process_times_.has_value()) {
    if (!manager->should_skip_latency_metrics() &&
        !manager->should_skip_startup_metrics()) {
      WaapUIMetricsService* service = GetService();
      CHECK(service);
      service->OnReloadButtonRendererProcessCreatedAndLaunched(
          renderer_process_times_->init_time,
          renderer_process_times_->launched_time);
    }
    renderer_process_times_.reset();
  }
}

// InitialWebUIWebContentsMetricsDispatcher:
WEB_CONTENTS_USER_DATA_KEY_IMPL(InitialWebUIWebContentsMetricsDispatcher);

InitialWebUIWebContentsMetricsDispatcher::
    InitialWebUIWebContentsMetricsDispatcher(content::WebContents* web_contents)
    : content::WebContentsUserData<InitialWebUIWebContentsMetricsDispatcher>(
          *web_contents) {}

InitialWebUIWebContentsMetricsDispatcher::
    ~InitialWebUIWebContentsMetricsDispatcher() = default;

WaapUIMetricsService* InitialWebUIWebContentsMetricsDispatcher::GetService() {
  // Use the profile of the window rather than the one of the WebContents,
  // which can be the shared TopChromeWebUIProfile that has no
  // `WaapUIMetricsService`.
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(&GetWebContents());
  return browser ? WaapUIMetricsService::Get(browser->GetProfile()) : nullptr;
}

InitialWebUIWindowMetricsManager*
InitialWebUIWebContentsMetricsDispatcher::GetWindowMetricsManager() {
  return InitialWebUIWindowMetricsManager::From(
      webui::GetBrowserWindowInterface(&GetWebContents()));
}

void InitialWebUIWebContentsMetricsDispatcher::OnManagerUnavailable() {
  if (!bwi_subscription_) {
    bwi_subscription_ = webui::RegisterBrowserWindowInterfaceChanged(
        &GetWebContents(),
        base::BindRepeating(
            &InitialWebUIWebContentsMetricsDispatcher::FlushMetricsToManager,
            base::Unretained(this)));
  }
}

// InitialWebUINativeMetricsDispatcher:
InitialWebUINativeMetricsDispatcher::InitialWebUINativeMetricsDispatcher(
    WaapUIMetricsService* metrics_service,
    InitialWebUIWindowMetricsManager* window_metrics_manager)
    : metrics_service_(metrics_service),
      window_metrics_manager_(window_metrics_manager) {
  CHECK(metrics_service_);
}

InitialWebUINativeMetricsDispatcher::~InitialWebUINativeMetricsDispatcher() =
    default;

WaapUIMetricsService* InitialWebUINativeMetricsDispatcher::GetService() {
  return metrics_service_;
}

InitialWebUIWindowMetricsManager*
InitialWebUINativeMetricsDispatcher::GetWindowMetricsManager() {
  return window_metrics_manager_;
}
