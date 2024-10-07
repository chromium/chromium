// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

class TabStripModel;

namespace tabs {
class TabModel;
}

namespace content {
class NavigationHandle;
}  // namespace content

class TabData : public TabStripModelObserver,
                public content::WebContentsObserver {
 public:
  // TODO(crbug.com/40070608) replace with opaque tab handle
  using TabID = int;

  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnTabDataUpdated(const TabData* tab_data) {}
    virtual void OnTabDataDestroyed(TabID tab_id) {}
  };

  explicit TabData(tabs::TabModel* tab);
  ~TabData() override;
  TabID tab_id() const { return tab_.raw_value(); }
  const TabStripModel* original_tab_strip_model() const {
    return original_tab_strip_model_;
  }
  TabStripModel* original_tab_strip_model() {
    return original_tab_strip_model_;
  }
  tabs::TabModel* tab() const { return tab_.Get(); }
  const GURL& original_url() const { return original_url_; }

  void AddObserver(Observer* new_observer);
  void RemoveObserver(Observer* new_observer);

  // Checks if the Tab is still valid for an organization. If allowed_group_id
  // is provided, will not exclude tabs on the basis of being part of that
  // group.
  bool IsValidForOrganizing(std::optional<tab_groups::TabGroupId>
                                allowed_group_id = std::nullopt) const;

  // TabStripModelObserver:
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void OnTabWillDiscardContents(tabs::TabInterface* tab,
                                content::WebContents* old_contents,
                                content::WebContents* new_contents);

  // Notifies observers of the tab data that it has been updated.
  void NotifyObserversOfUpdate();

  raw_ptr<TabStripModel> original_tab_strip_model_;
  tabs::TabHandle tab_;
  const GURL original_url_;

  base::ObserverList<Observer>::Unchecked observers_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_DATA_H_
