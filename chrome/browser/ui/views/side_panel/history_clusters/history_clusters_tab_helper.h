// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace side_panel {

class HistoryClustersTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HistoryClustersTabHelper> {
 public:
  class Delegate {
   public:
    // Shows the Journeys side panel with `query` pre-populated.
    virtual void ShowJourneysSidePanel(const std::string& query) = 0;

    virtual ~Delegate() = default;
  };

  ~HistoryClustersTabHelper() override;

  HistoryClustersTabHelper(const HistoryClustersTabHelper&) = delete;
  HistoryClustersTabHelper& operator=(const HistoryClustersTabHelper&) = delete;

  // Opens Side Panel to the Journeys UI with the given query.
  virtual void ShowJourneysSidePanel(const std::string& query);

 protected:
  explicit HistoryClustersTabHelper(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<HistoryClustersTabHelper>;

  std::unique_ptr<Delegate> delegate_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace side_panel

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
