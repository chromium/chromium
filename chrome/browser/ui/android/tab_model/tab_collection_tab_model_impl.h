// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_COLLECTION_TAB_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_COLLECTION_TAB_MODEL_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"

class Profile;
class TabAndroid;
class TabGroup;

namespace base {
class Token;
}  // namespace base

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

namespace tabs {
class TabGroupTabCollection;
class TabStripCollection;

// The C++ portion of TabCollectionTabModelImpl.java. Note this is intentionally
// a different entity from TabModelJniBridge as that class is shared between the
// non-tab collection and tab collection implementations. In future, after tab
// launches, it may be prudent to merge the C++ objects.
class TabCollectionTabModelImpl {
 public:
  TabCollectionTabModelImpl(JNIEnv* env,
                            const jni_zero::JavaRef<jobject>& java_object,
                            Profile* profile);
  ~TabCollectionTabModelImpl();
  // Called by Java to destroy this object. Do not call directly in C++.
  void Destroy(JNIEnv* env);

  TabCollectionTabModelImpl(const TabCollectionTabModelImpl&) = delete;
  TabCollectionTabModelImpl& operator=(const TabCollectionTabModelImpl&) =
      delete;

  // Returns the total number of tabs in the collection, including
  // sub-collections.
  int GetTabCountRecursive(JNIEnv* env) const;

  // Returns the recursive index of the given tab, or -1 if not found.
  int GetIndexOfTabRecursive(JNIEnv* env, TabAndroid* j_tab_android) const;

  // Recurses until reaching the given index. Returns null if not found.
  TabAndroid* GetTabAtIndexRecursive(JNIEnv* env, size_t index) const;

  // Moves a tab updating its group or pinned state if applicable. Returns the
  // final index of the tab.
  int MoveTabRecursive(JNIEnv* env,
                       size_t current_index,
                       size_t new_index,
                       const std::optional<base::Token>& j_new_tab_group_id,
                       bool new_is_pinned);

  // Adds a tab to the tab model. Returns the final index of the tab.
  int AddTabRecursive(JNIEnv* env,
                      TabAndroid* tab,
                      size_t index,
                      const std::optional<base::Token>& j_tab_group_id,
                      bool is_attaching_group,
                      bool is_pinned);

  // Removes a list of tabs from the tab model.
  void RemoveTabRecursive(JNIEnv* env, TabAndroid* tab);

  // Create tab group.
  void CreateTabGroup(JNIEnv* env,
                      const base::Token& tab_group_id,
                      const std::u16string& tab_group_title,
                      jint j_color_id,
                      bool is_collapsed);

  // Moves the tab group to a the new index. Returns the final index of the
  // group.
  int MoveTabGroupTo(JNIEnv* env,
                     const base::Token& tab_group_id,
                     int to_index);

  // Returns the tabs in a group. If the group is not found, returns an empty
  // vector.
  std::vector<TabAndroid*> GetTabsInGroup(JNIEnv* env,
                                          const base::Token& token);

  // Returns the number of tabs in a group. If the group is not found, returns
  // 0.
  int GetTabCountForGroup(JNIEnv* env, const base::Token& token);

  // Returns whether a tab group with tabs exists.
  bool TabGroupExists(JNIEnv* env, const base::Token& token);

  // Returns the number of individual tabs and tab groups.
  int GetIndividualTabAndGroupCount(JNIEnv* env);

  // Returns the number of tab groups.
  int GetTabGroupCount(JNIEnv* env);

  // Returns the index of a tab within its group. Returns -1 if tab is not in a
  // group or not found.
  int GetIndexOfTabInGroup(JNIEnv* env,
                           TabAndroid* tab,
                           const base::Token& token);

  // Update tab group visual data.
  void UpdateTabGroupVisualData(
      JNIEnv* env,
      const base::Token& tab_group_id,
      const std::optional<std::u16string>& tab_group_title,
      const std::optional<jint>& j_color_id,
      const std::optional<bool>& is_collapsed);

  // Getters for tab group visual data.
  std::u16string GetTabGroupTitle(JNIEnv* env, const base::Token& tab_group_id);
  jint GetTabGroupColor(JNIEnv* env, const base::Token& tab_group_id);
  bool GetTabGroupCollapsed(JNIEnv* env, const base::Token& tab_group_id);

  // Checks if a detached tab group exists.
  bool DetachedTabGroupExists(JNIEnv* env, const base::Token& tab_group_id);

  // Closes a detached tab group.
  void CloseDetachedTabGroup(JNIEnv* env, const base::Token& tab_group_id);

  // Gets a list of all tabs.
  std::vector<TabAndroid*> GetAllTabs(JNIEnv* env);

  // Gets a list of all tab group IDs.
  std::vector<base::Token> GetAllTabGroupIds(JNIEnv* env);

  // Gets a list of representative tabs.
  std::vector<TabAndroid*> GetRepresentativeTabList(JNIEnv* env);

  // Sets the last shown tab for a group.
  void SetLastShownTabForGroup(JNIEnv* env,
                               const base::Token& group_id,
                               TabAndroid* tab_android);

  // Gets the last shown tab for a group.
  TabAndroid* GetLastShownTabForGroup(JNIEnv* env, const base::Token& group_id);

  // Returns the index of the first non-pinned tab.
  int GetIndexOfFirstNonPinnedTab(JNIEnv* env);

  // Returns the TabStripCollection associated with this TabModel.
  tabs::TabStripCollection* GetTabStripCollection(JNIEnv* env);

 private:
  // Returns a safe index for adding or moving a tab or tab group.
  // `is_tab_group` is used to indicate if we are working with a tab or a tab
  // group. `current_index` is the current index of the tab; it should be
  // nullopt when adding a new tab to the collection. `proposed_index` is the
  // index at which the tab or group is proposed to be moved or added. The
  // returned value should be used instead. `tab_group_id` and `is_pinned` are
  // the collection the tab or group will be in after the add or move operation.
  size_t GetSafeIndex(bool is_tab_group,
                      const std::optional<size_t>& current_index,
                      size_t proposed_index,
                      const std::optional<tab_groups::TabGroupId>& tab_group_id,
                      bool is_pinned) const;
  std::optional<tab_groups::TabGroupId> GetGroupIdAt(size_t index) const;
  TabGroupTabCollection* GetTabGroupCollectionChecked(
      const tab_groups::TabGroupId& tab_group_id,
      bool allow_detached = false) const;
  TabGroup* GetTabGroupChecked(const tab_groups::TabGroupId& tab_group_id,
                               bool allow_detached = false) const;
  const tab_groups::TabGroupVisualData* GetTabGroupVisualDataChecked(
      const tab_groups::TabGroupId& tab_group_id,
      bool allow_detached = false) const;

  JavaObjectWeakGlobalRef java_object_;
  raw_ptr<Profile> profile_;

  // Always valid until destroyed.
  std::unique_ptr<tabs::TabStripCollection> tab_strip_collection_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_COLLECTION_TAB_MODEL_IMPL_H_
