// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_WINDOW_ACTIVITY_WATCHER_H_
#define CHROME_BROWSER_UI_TABS_WINDOW_ACTIVITY_WATCHER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace tab_ranker {
struct WindowFeatures;
}  // namespace tab_ranker

// Observes browser window activity in order to log WindowMetrics UKMs for
// browser events relative to tab activation and discarding.
// Multiple tabs in the same browser can refer to the same WindowMetrics entry.
// Must be used on the UI thread.
// TODO(michaelpg): Observe app and ARC++ windows as well.
class WindowActivityWatcher : public BrowserListObserver {
 public:
  class BrowserWatcher;

  // Returns the single instance, creating it if necessary.
  static WindowActivityWatcher* GetInstance();

  // Returns a populated WindowFeatures for the browser.
  static tab_ranker::WindowFeatures CreateWindowFeatures(
      const Browser* browser);

 private:
  friend class base::NoDestructor<WindowActivityWatcher>;

  WindowActivityWatcher();
  ~WindowActivityWatcher() override;

  bool ShouldTrackBrowser(Browser* browser);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // Per-browser observers responsible for tracking the tab strip and logging
  // new UKM entries on changes.
  base::flat_map<Browser*, std::unique_ptr<BrowserWatcher>> browser_watchers_;

  DISALLOW_COPY_AND_ASSIGN(WindowActivityWatcher);
};

#endif  // CHROME_BROWSER_UI_TABS_WINDOW_ACTIVITY_WATCHER_H_
