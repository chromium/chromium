// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UMA_BROWSING_ACTIVITY_OBSERVER_H_
#define CHROME_BROWSER_UI_UMA_BROWSING_ACTIVITY_OBSERVER_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_stats_recorder.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chrome {

// This object is instantiated when the first Browser object is added to the
// list and delete when the last one is removed. It watches for loads and
// creates histograms of some global object counts.
class UMABrowsingActivityObserver : public content::NotificationObserver {
 public:
  UMABrowsingActivityObserver(const UMABrowsingActivityObserver&) = delete;
  UMABrowsingActivityObserver& operator=(const UMABrowsingActivityObserver&) =
      delete;

  static void Init();

 private:
  UMABrowsingActivityObserver();
  ~UMABrowsingActivityObserver() override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void OnAppTerminating() const;

  // Calculates the time from an update being visible to the browser and
  // the browser restarting or quitting and logs it.
  void LogTimeBeforeUpdate() const;

  // Counts the number of active RenderProcessHosts and logs them.
  void LogRenderProcessHostCount() const;

  // Counts the number of tabs in each browser window and logs them. This is
  // different than the number of WebContents objects since WebContents objects
  // can be used for popups and in dialog boxes. We're just counting toplevel
  // tabs here.
  void LogBrowserTabCount() const;

  // Maps |total_tab_count| to the corresponding histogram bucket with the
  // proper name suffix.
  std::string AppendTabBucketCountToHistogramName(int total_tab_count) const;

  content::NotificationRegistrar registrar_;
  TabStripModelStatsRecorder tab_recorder_;
  base::CallbackListSubscription subscription_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_UMA_BROWSING_ACTIVITY_OBSERVER_H_
