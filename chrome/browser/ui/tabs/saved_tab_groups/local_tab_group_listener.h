// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_LOCAL_TAB_GROUP_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_LOCAL_TAB_GROUP_LISTENER_H_

#include "base/guid.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}

class SavedTabGroup;
class SavedTabGroupModel;
class TabStripModel;

// Keeps a saved tab group up to date as it's changed locally.
class LocalTabGroupListener {
 public:
  LocalTabGroupListener(
      tab_groups::TabGroupId local_id,
      base::GUID saved_guid,
      SavedTabGroupModel* model,
      std::vector<std::pair<content::WebContents*, base::GUID>> mapping);
  virtual ~LocalTabGroupListener();

  // Updates the saved group with the new tab and tracks it for further changes.
  void AddWebContents(content::WebContents* web_contents,
                      TabStripModel* tab_strip_model,
                      int index);
  // If `web_contents` is in this listener's local tab group, removes it from
  // the saved tab group and stops tracking it.
  void RemoveWebContentsIfPresent(content::WebContents* web_contents);

  // Testing Accessors.
  std::unordered_map<content::WebContents*, SavedTabGroupWebContentsListener>&
  GetWebContentsTokenMapForTesting() {
    return web_contents_to_tab_id_map_;
  }

 private:
  const SavedTabGroup* saved_group() const { return model_->Get(saved_guid_); }

  std::unordered_map<content::WebContents*, SavedTabGroupWebContentsListener>
      web_contents_to_tab_id_map_;
  const raw_ptr<SavedTabGroupModel> model_;
  const tab_groups::TabGroupId local_id_;
  const base::GUID saved_guid_;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_LOCAL_TAB_GROUP_LISTENER_H_
