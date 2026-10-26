// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_METRICS_DISPATCHER_H_
#define CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_METRICS_DISPATCHER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_user_data.h"

class InitialWebUIWindowMetricsManager;
class WaapUIMetricsService;

namespace content {
class WebContents;
}

class InitialWebUIWebContentsMetricsDispatcher;

// Base class for dispatching InitialWebUI latency metrics for either a
// WebContents or a native window element (e.g. ReloadButton).
//
// Receives milestones (creation, commit, first paint, first contentful paint)
// and handles the common metrics logic:
// - Directs startup metrics to `WaapUIMetricsService` and window-scoped
//   metrics to `InitialWebUIWindowMetricsManager`.
// - Buffers prewarmed `OnCommit()` process times until the
//   `InitialWebUIWindowMetricsManager` is available.
class InitialWebUIMetricsDispatcher {
 public:
  // Convenience forwarders for WebContents-based dispatchers.
  static InitialWebUIWebContentsMetricsDispatcher* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  virtual ~InitialWebUIMetricsDispatcher();

  InitialWebUIMetricsDispatcher(const InitialWebUIMetricsDispatcher&) = delete;
  InitialWebUIMetricsDispatcher& operator=(
      const InitialWebUIMetricsDispatcher&) = delete;

  // Called when the initial WebUI element is created.
  virtual void OnCreated();

  // Called when the navigation is committed with renderer process timestamps.
  virtual void OnCommit(base::TimeTicks init_time,
                        base::TimeTicks launched_time);

  // Called when the initial WebUI element paints its first frame.
  virtual void OnFirstPaint(base::TimeTicks paint_time);

  // Called when the initial WebUI element paints its first contentful frame.
  virtual void OnFirstContentfulPaint(base::TimeTicks fcp_time);

 protected:
  InitialWebUIMetricsDispatcher();

  // Returns the service of the profile of the window that metrics are
  // reported to. Only called while `GetWindowMetricsManager()` returns a
  // manager, and never returns null then.
  virtual WaapUIMetricsService* GetService() = 0;
  virtual InitialWebUIWindowMetricsManager* GetWindowMetricsManager() = 0;

  virtual void OnManagerUnavailable() {}

  void FlushMetricsToManager();

 private:
  struct RendererProcessTimes {
    base::TimeTicks init_time;
    base::TimeTicks launched_time;
  };

  std::optional<RendererProcessTimes> renderer_process_times_;
  bool is_first_paint_handled_ = false;
  bool is_first_contentful_paint_handled_ = false;
};

// Dispatcher attached to a WebContents (used by WebUI surfaces).
// Bridges the lifecycle mismatch where a WebContents may be pre-warmed or
// created before being attached to a BrowserWindowInterface.
class InitialWebUIWebContentsMetricsDispatcher
    : public InitialWebUIMetricsDispatcher,
      public content::WebContentsUserData<
          InitialWebUIWebContentsMetricsDispatcher> {
 public:
  using content::WebContentsUserData<
      InitialWebUIWebContentsMetricsDispatcher>::FromWebContents;
  using content::WebContentsUserData<
      InitialWebUIWebContentsMetricsDispatcher>::CreateForWebContents;

  ~InitialWebUIWebContentsMetricsDispatcher() override;

 protected:
  // InitialWebUIMetricsDispatcher:
  WaapUIMetricsService* GetService() override;
  InitialWebUIWindowMetricsManager* GetWindowMetricsManager() override;
  void OnManagerUnavailable() override;

 private:
  explicit InitialWebUIWebContentsMetricsDispatcher(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      InitialWebUIWebContentsMetricsDispatcher>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::CallbackListSubscription bwi_subscription_;
};

// Dispatcher for native Views (e.g. Views ReloadButton).
// Directly associated with a WaapUIMetricsService and
// InitialWebUIWindowMetricsManager.
class InitialWebUINativeMetricsDispatcher
    : public InitialWebUIMetricsDispatcher {
 public:
  InitialWebUINativeMetricsDispatcher(
      WaapUIMetricsService* metrics_service,
      InitialWebUIWindowMetricsManager* window_metrics_manager);
  ~InitialWebUINativeMetricsDispatcher() override;

 protected:
  // InitialWebUIMetricsDispatcher:
  WaapUIMetricsService* GetService() override;
  InitialWebUIWindowMetricsManager* GetWindowMetricsManager() override;

 private:
  // Never null.
  const raw_ptr<WaapUIMetricsService> metrics_service_;
  const raw_ptr<InitialWebUIWindowMetricsManager> window_metrics_manager_;
};

#endif  // CHROME_BROWSER_UI_WAAP_INITIAL_WEBUI_METRICS_DISPATCHER_H_
