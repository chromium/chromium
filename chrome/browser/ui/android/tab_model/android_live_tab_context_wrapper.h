// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_WRAPPER_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_WRAPPER_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/android/historical_tab_saver.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "components/sessions/core/live_tab.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

class TabModel;

// Subclass for supplying tabs and group data for bulk tab closures.
class AndroidLiveTabContextCloseWrapper : public AndroidLiveTabContext {
 public:
  // AndroidLiveTabContextCloseWrapper wraps AndroidLiveTabContext for
  // closing tabs. In this mode:
  // - `closed_tabs` is the list of tabs closed in a particular close event
  //   handled by the delegate
  // - `id_to_tab_group` is a mapping from a closed Android Tab's ID to a
  //   TabGroupId if it is part of a group. This TabGroupId is not the same ID
  //   as an Android Group ID and is used as a proxy. However, it is a 1:1
  //   relationship.
  // - `tab_group_visual_data` is used to supply `title()` of a tab group. One
  //   of these entries must exist for every unique TabGroupId in
  //   `id_to_tab_group`. If the title is null in Java use "".
  AndroidLiveTabContextCloseWrapper(
      TabModel* tab_model,
      std::vector<TabAndroid*>&& closed_tabs,
      std::map<int, tab_groups::TabGroupId>&& tab_id_to_tab_group,
      std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>&&
          tab_group_visual_data,
      std::vector<WebContentsStateByteBuffer>&& web_contents_state);
  ~AndroidLiveTabContextCloseWrapper() override;

  AndroidLiveTabContextCloseWrapper(const AndroidLiveTabContextCloseWrapper&) =
      delete;
  AndroidLiveTabContextCloseWrapper& operator=(
      const AndroidLiveTabContextCloseWrapper&) = delete;

  // Gets the number of closing tabs handled by this wrapper.
  int GetTabCount() const override;

  // Stubs getting the currently selected index.
  int GetSelectedIndex() const override;

  // Gets the sessions::LiveTab corresponding to the tab at
  // `relative_index` into `closed_tabs`. If the TabAndroid is frozen a
  // temporary WebContents without a renderer will be created. Note that only
  // one sessions::LiveTab* vended from this delegate should be alive at any
  // time as the temporary WebContents may be deleted upon next call to this
  // method as it's lifetime is managed by this wrapper class.
  sessions::LiveTab* GetLiveTabAt(int relative_index) const override;

  // Gets the TabGroupId for the Tab at `relative_index` into `closed_tabs` or
  // returns nullopt otherwise.
  absl::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int relative_index) const override;

  // Gets the visual data for `group_id` if it exists or nullptr otherwise.
  const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const override;

 private:
  TabAndroid* GetTabAt(int relative_index) const;

  // List of indices to close for using BrowserClosing to proxy bulk
  // closure.
  std::vector<TabAndroid*> closed_tabs_;

  // Maps tab IDs to tab groups.
  std::map<int, tab_groups::TabGroupId> tab_id_to_tab_group_;

  // Maps a group ID to its visual data (only a title on Android).
  std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>
      tab_group_visual_data_;

  // List of webContentStates to close linked by tab index for bulk closure.
  std::vector<WebContentsStateByteBuffer> web_contents_state_;

  // The most recently unfrozen web contents. Mutable as const signature methods
  // modify this field (constness inherited from LiveTabContext).
  mutable std::unique_ptr<historical_tab_saver::ScopedWebContents>
      scoped_web_contents_;
};

// Subclass for extracting group data for bulk tab restores.
class AndroidLiveTabContextRestoreWrapper : public AndroidLiveTabContext {
 public:
  explicit AndroidLiveTabContextRestoreWrapper(TabModel* tab_model);
  ~AndroidLiveTabContextRestoreWrapper() override;

  AndroidLiveTabContextRestoreWrapper(
      const AndroidLiveTabContextRestoreWrapper&) = delete;
  AndroidLiveTabContextRestoreWrapper& operator=(
      const AndroidLiveTabContextRestoreWrapper&) = delete;

  // A group of tabs. Used for the purpose of converting between
  // TabRestoreService and Android representations of Tab Groups.
  struct TabGroup {
    TabGroup();
    ~TabGroup();

    TabGroup(const TabGroup&) = delete;
    TabGroup& operator=(const TabGroup&) = delete;

    TabGroup(TabGroup&&);
    TabGroup& operator=(TabGroup&&);

    // Visual data for the group (only a title for Android).
    tab_groups::TabGroupVisualData visual_data;

    // Android Tab IDs for members of the group.
    std::vector<int> tab_ids;
  };

  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  sessions::LiveTab* AddRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      absl::optional<tab_groups::TabGroupId> group,
      const tab_groups::TabGroupVisualData& group_visual_data,
      bool select,
      bool pin,
      const sessions::PlatformSpecificTabData* storage_namespace,
      const sessions::SerializedUserAgentOverride& user_agent_override,
      const std::map<std::string, std::string>& extra_data,
      const SessionID* tab_id) override;

  // Returns the TabGroup data aggregated via AddRestoredTab.
  const std::map<tab_groups::TabGroupId, TabGroup>& GetTabGroups();

 private:
  // Mapping of restored tab group ids to Android tab groups.
  std::map<tab_groups::TabGroupId, TabGroup> tab_groups_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_WRAPPER_H_
