// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UMA_BROWSING_ACTIVITY_OBSERVER_H_
#define CHROME_BROWSER_UI_UMA_BROWSING_ACTIVITY_OBSERVER_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_stats_recorder.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace chrome {

// This object is instantiated during startup, before the first Browser object
// is added to the list and deleted during shutdown. It watches for loads and
// creates histograms of some global object counts.
class UMABrowsingActivityObserver {
 public:
  UMABrowsingActivityObserver(const UMABrowsingActivityObserver&) = delete;
  UMABrowsingActivityObserver& operator=(const UMABrowsingActivityObserver&) =
      delete;

  static void Init();

  // Notifies `UMABrowsingActivityObserver` with tab related events.
  class TabHelper : public content::WebContentsObserver,
                    public content::WebContentsUserData<TabHelper> {
   public:
    TabHelper(const TabHelper&) = delete;
    TabHelper& operator=(const TabHelper&) = delete;
    ~TabHelper() override;

    // content::WebContentsObserver
    void NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) override;

   private:
    explicit TabHelper(content::WebContents* web_contents);
    friend class content::WebContentsUserData<TabHelper>;
    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };

 private:
  UMABrowsingActivityObserver();
  ~UMABrowsingActivityObserver();

  void OnNavigationEntryCommitted(
      content::WebContents* web_contents,
      const content::LoadCommittedDetails& load_details) const;

  void OnAppTerminating() const;

  // Calculates the time from an update being visible to the browser and
  // the browser restarting or quitting and logs it.
  void LogTimeBeforeUpdate() const;

  // Counts the number of tabs in each browser window and logs them. This is
  // different than the number of WebContents objects since WebContents objects
  // can be used for popups and in dialog boxes. We're just counting toplevel
  // tabs here.
  void LogBrowserTabCount() const;

  const TabStripModelStatsRecorder tab_recorder_;
  base::CallbackListSubscription subscription_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_UMA_BROWSING_ACTIVITY_OBSERVER_H_
