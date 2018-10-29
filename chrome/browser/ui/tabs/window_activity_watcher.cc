// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/window_activity_watcher.h"

#include "base/logging.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "chrome/browser/resource_coordinator/tab_ranker/window_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using metrics::WindowMetricsEvent;
using tab_ranker::WindowFeatures;

namespace {

// Sets feature values that are dependent on the current window state.
void UpdateWindowFeatures(const Browser* browser,
                          WindowFeatures* window_features,
                          bool is_active) {
  DCHECK(browser->window());

  // TODO(michaelpg): Observe the show state and log when it changes.
  if (browser->window()->IsFullscreen())
    window_features->show_state = WindowMetricsEvent::SHOW_STATE_FULLSCREEN;
  else if (browser->window()->IsMinimized())
    window_features->show_state = WindowMetricsEvent::SHOW_STATE_MINIMIZED;
  else if (browser->window()->IsMaximized())
    window_features->show_state = WindowMetricsEvent::SHOW_STATE_MAXIMIZED;
  else
    window_features->show_state = WindowMetricsEvent::SHOW_STATE_NORMAL;

  window_features->is_active = is_active;
  window_features->tab_count = browser->tab_strip_model()->count();
}

// Returns a populated WindowFeatures for the browser.
// |is_active| is provided because IsActive() may be incorrect while browser
// activation is changing (namely, when deactivating a window on Windows).
WindowFeatures CreateWindowFeatures(const Browser* browser, bool is_active) {
  WindowMetricsEvent::Type window_type = WindowMetricsEvent::TYPE_UNKNOWN;
  switch (browser->type()) {
    case Browser::TYPE_TABBED:
      window_type = WindowMetricsEvent::TYPE_TABBED;
      break;
    case Browser::TYPE_POPUP:
      window_type = WindowMetricsEvent::TYPE_POPUP;
      break;
    default:
      NOTREACHED();
  }

  WindowFeatures window_features(browser->session_id(), window_type);
  UpdateWindowFeatures(browser, &window_features, is_active);
  return window_features;
}

// Logs a UKM entry with the metrics from |window_features|.
void LogWindowMetricsUkmEntry(const WindowFeatures& window_features) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::builders::TabManager_WindowMetrics entry(ukm::AssignNewSourceId());
  entry.SetWindowId(window_features.window_id.id())
      .SetIsActive(window_features.is_active)
      .SetShowState(window_features.show_state)
      .SetType(window_features.type);

  // Bucketize values for privacy considerations. Use a spacing factor that
  // ensures values up to 3 can be logged exactly; after that, the precise
  // number becomes less significant.
  int tab_count = window_features.tab_count;
  if (tab_count > 3)
    tab_count = ukm::GetExponentialBucketMin(tab_count, 1.5);
  entry.SetTabCount(tab_count);

  entry.Record(ukm_recorder);
}

}  // namespace

// Observes a browser window's tab strip and logs a WindowMetrics UKM event for
// the window upon changes to metrics like TabCount.
class WindowActivityWatcher::BrowserWatcher : public TabStripModelObserver {
 public:
  explicit BrowserWatcher(Browser* browser)
      : browser_(browser), observer_(this) {
    DCHECK(!browser->profile()->IsOffTheRecord());
    MaybeLogWindowMetricsUkmEntry();
    observer_.Add(browser->tab_strip_model());
  }

  ~BrowserWatcher() override = default;

  void MaybeLogWindowMetricsUkmEntry() {
    MaybeLogWindowMetricsUkmEntry(browser_->window()->IsActive());
  }

  // Logs a new WindowMetrics entry to the UKM recorder if the entry would be
  // different than the last one we logged.
  // |is_active| is provided because IsActive() may be incorrect while browser
  // activation is changing (namely, when deactivating a window on Windows).
  void MaybeLogWindowMetricsUkmEntry(bool is_active) {
    // Do nothing if the window has no tabs (which can happen when a window is
    // opened, before a tab is added) or is being closed.
    if (browser_->tab_strip_model()->empty() ||
        browser_->tab_strip_model()->closing_all()) {
      return;
    }

    if (!last_window_features_) {
      last_window_features_.emplace(
          ::CreateWindowFeatures(browser_, is_active));
      LogWindowMetricsUkmEntry(last_window_features_.value());
      return;
    }

    // Copy old state to compare with.
    WindowFeatures old_features(last_window_features_.value());
    UpdateWindowFeatures(browser_, &last_window_features_.value(), is_active);

    // We only need to create a new UKM entry if the metrics have changed.
    if (old_features != last_window_features_.value())
      LogWindowMetricsUkmEntry(last_window_features_.value());
  }

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted &&
        change.type() != TabStripModelChange::kRemoved)
      return;

    MaybeLogWindowMetricsUkmEntry();
  }

  // The browser whose tab strip we track. WindowActivityWatcher should ensure
  // this outlives us.
  Browser* browser_;

  // The most recent WindowFeatures entry logged. We only log a new UKM entry if
  // some metric value has changed.
  base::Optional<WindowFeatures> last_window_features_;

  // Used to update the tab count for browser windows.
  ScopedObserver<TabStripModel, TabStripModelObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWatcher);
};

// static
WindowActivityWatcher* WindowActivityWatcher::GetInstance() {
  static base::NoDestructor<WindowActivityWatcher> instance;
  return instance.get();
}

// static
WindowFeatures WindowActivityWatcher::CreateWindowFeatures(
    const Browser* browser) {
  DCHECK(browser->window());
  return ::CreateWindowFeatures(browser, browser->window()->IsActive());
}

WindowActivityWatcher::WindowActivityWatcher() {
  BrowserList::AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);
}

WindowActivityWatcher::~WindowActivityWatcher() {
  BrowserList::RemoveObserver(this);
}

bool WindowActivityWatcher::ShouldTrackBrowser(Browser* browser) {
  // Don't track incognito browsers. This is also enforced by UKM.
  return !browser->profile()->IsOffTheRecord();
}

void WindowActivityWatcher::OnBrowserAdded(Browser* browser) {
  if (ShouldTrackBrowser(browser))
    browser_watchers_[browser] = std::make_unique<BrowserWatcher>(browser);
}

void WindowActivityWatcher::OnBrowserRemoved(Browser* browser) {
  browser_watchers_.erase(browser);
}

void WindowActivityWatcher::OnBrowserSetLastActive(Browser* browser) {
  // The browser may not have a window yet if activation calls happen during
  // initialization.
  // TODO(michaelpg): The browser window check should be unnecessary
  // (https://crbug.com/811191, https://crbug.com/811243).
  if (ShouldTrackBrowser(browser) && browser->window())
    browser_watchers_[browser]->MaybeLogWindowMetricsUkmEntry();
}

void WindowActivityWatcher::OnBrowserNoLongerActive(Browser* browser) {
  // The browser may not have a window yet if activation calls happen during
  // initialization.
  // TODO(michaelpg): The browser window check should be unnecessary
  // (https://crbug.com/811191, https://crbug.com/811243).
  if (!ShouldTrackBrowser(browser) || !browser->window())
    return;

#if defined(USE_AURA)
  // On some platforms, the window is hidden (and deactivated) before it starts
  // closing. Unless the window is minimized, assume that being deactivated
  // while hidden means it's about to close, and don't log in that case.
  if (browser->window()->GetNativeWindow() &&
      !browser->window()->GetNativeWindow()->IsVisible() &&
      !browser->window()->IsMinimized()) {
    return;
  }
#endif

  // On Windows, the browser window's IsActive() may still return true until the
  // WM updates the focused window, and BrowserList::GetLastActive() still
  // returns this browser until another one is activated. So we explicitly pass
  // along that the window should be considered inactive.
  browser_watchers_[browser]->MaybeLogWindowMetricsUkmEntry(
      /*is_active=*/false);
}
