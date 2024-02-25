// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

class TabStripModel;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class TabData : public TabStripModelObserver,
                public content::WebContentsObserver {
 public:
  // TODO(1476012) replace with opaque tab handle
  using TabID = int;

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnTabDataUpdated(const TabData* tab_data) {}
    virtual void OnTabDataDestroyed(TabID tab_id) {}
  };

  TabData(TabStripModel* model, content::WebContents* web_contents);
  ~TabData() override;
  const TabID& tab_id() const { return tab_id_; }
  const TabStripModel* original_tab_strip_model() const {
    return original_tab_strip_model_;
  }
  TabStripModel* original_tab_strip_model() {
    return original_tab_strip_model_;
  }
  const content::WebContents* web_contents() const { return web_contents_; }
  content::WebContents* web_contents() { return web_contents_; }
  const GURL& original_url() const { return original_url_; }

  void AddObserver(Observer* new_observer);
  void RemoveObserver(Observer* new_observer);

  // Checks if the Tab is still valid for an organization.
  bool IsValidForOrganizing() const;

  // TabStripModelObserver:
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Notifies observers of the tab data that it has been updated.
  void NotifyObserversOfUpdate();

  const TabID tab_id_;
  raw_ptr<TabStripModel> original_tab_strip_model_;
  raw_ptr<content::WebContents> web_contents_;
  const GURL original_url_;

  base::ObserverList<Observer>::Unchecked observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_
