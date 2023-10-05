// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_LOCAL_TAB_GROUP_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_LOCAL_TAB_GROUP_LISTENER_H_

#include "base/uuid.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}

class SavedTabGroup;
class SavedTabGroupModel;
class TabStripModel;
class Browser;

// Keeps a saved tab group up to date as it's changed locally.
class LocalTabGroupListener {
 public:
  LocalTabGroupListener(
      tab_groups::TabGroupId local_id,
      base::Uuid saved_guid,
      SavedTabGroupModel* model,
      std::map<content::WebContents*, base::Uuid> web_contents_to_uuid);
  virtual ~LocalTabGroupListener();

  // Pauses listening to changes to the local tab group. Call this before
  // beginning a multi-step operation that will have no net effect on the group
  // (e.g. moving it to another window).
  void PauseTracking();

  // Resumes listening to changes to the local tab group. The tab group must be
  // in the same configuration it was in when PauseTracking was called (this is
  // CHECKed).
  void ResumeTracking();

  void UpdateVisualDataFromLocal(
      const TabGroupChange::VisualsChange* visuals_change);

  // Updates the saved group with the new tab and tracks it for further changes.
  void AddWebContentsFromLocal(content::WebContents* web_contents,
                               TabStripModel* tab_strip_model,
                               int index);

  // Replaces the webcontents associated with the SavedTabGroupTab.
  void OnReplaceWebContents(content::WebContents* old_web_contents,
                            content::WebContents* new_web_contents);

  // Moves the SavedTab associated with `web_contents` in the TabStripModel to
  // its new relative position in the SavedTabGroup.
  void MoveWebContentsFromLocal(TabStripModel* tab_strip_model,
                                content::WebContents* web_contents,
                                int tabstrip_index_of_moved_tab);

  // Whether the local and saved groups this listener is connecting still exist.
  enum class Liveness {
    kGroupExists,
    kGroupDeleted,
  };

  // If `web_contents` is in this listener's local tab group, removes it from
  // the saved tab group and stops tracking it. Returns whether the local group
  // this is tracking still exists after `web_contents`' removal.
  [[nodiscard]] Liveness MaybeRemoveWebContentsFromLocal(
      content::WebContents* web_contents);

  // The saved group was deleted, so close the local group.
  void GroupRemovedFromSync();

  // Updates the local group to match the current state of the saved group.
  // Returns whether the local group still exists after this update.
  [[nodiscard]] Liveness UpdateFromSync();

  // Testing Accessors.
  std::unordered_map<content::WebContents*, SavedTabGroupWebContentsListener>&
  GetWebContentsTokenMapForTesting() {
    return web_contents_to_tab_id_map_;
  }

 private:
  // Updates `local_tab` to match `saved_tab`, and ensures it is at
  // `target_index_in_tab_strip` in `tab_strip_model`.
  void MatchLocalTabToSavedTab(SavedTabGroupTab saved_tab,
                               content::WebContents* local_tab,
                               TabStripModel* tab_strip_model,
                               int target_index_in_tab_strip);
  void OpenWebContentsFromSync(SavedTabGroupTab tab,
                               Browser* browser,
                               int index_in_tabstrip);

  // Removes any tabs in the local group that aren't in the saved group.
  void RemoveLocalWebContentsNotInSavedGroup();

  // Removes the WebContents from the mapping and removes the corresponding tab
  // from the group in the Tabstrip then closing it if should_close_tab is true.
  void RemoveWebContentsFromSync(content::WebContents* contents,
                                 bool should_close_tab);

  const SavedTabGroup* saved_group() const { return model_->Get(saved_guid_); }

  // Whether local tab group changes will be ignored (`paused_` is true) or
  // reflected in the saved group (`paused_` is false).
  bool paused_ = false;

  std::unordered_map<content::WebContents*, SavedTabGroupWebContentsListener>
      web_contents_to_tab_id_map_;
  const raw_ptr<SavedTabGroupModel> model_;
  const tab_groups::TabGroupId local_id_;
  const base::Uuid saved_guid_;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_LOCAL_TAB_GROUP_LISTENER_H_
