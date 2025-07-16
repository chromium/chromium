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

namespace base {
class Token;
}  // namespace base

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

namespace tabs {
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

  // Moves a tab updating its group or pinned state if applicable.
  int MoveTabRecursive(JNIEnv* env,
                       size_t current_index,
                       size_t new_index,
                       const std::optional<base::Token>& j_new_tab_group_id,
                       bool new_is_pinned);

  // Adds a tab to the tab model.
  void AddTabRecursive(JNIEnv* env,
                       TabAndroid* tab,
                       size_t index,
                       const std::optional<base::Token>& j_tab_group_id,
                       bool is_pinned);

  // Removes a list of tabs from the tab model.
  void RemoveTabRecursive(JNIEnv* env, TabAndroid* tab);

  // Create tab group.
  void CreateTabGroup(JNIEnv* env,
                      const base::Token& tab_group_id,
                      const std::u16string& tab_group_title,
                      jint j_color_id,
                      bool is_collapsed);

  // Moves the tab group to a the new index.
  void MoveTabGroupTo(JNIEnv* env,
                      const base::Token& tab_group_id,
                      int to_index);

  // Returns the tabs in a group. If the group is not found, returns an empty
  // vector.
  std::vector<TabAndroid*> GetTabsInGroup(JNIEnv* env,
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

  // Closes a detached tab group.
  void CloseDetachedTabGroup(JNIEnv* env, const base::Token& tab_group_id);

  // Gets a list of all tabs.
  std::vector<TabAndroid*> GetAllTabs(JNIEnv* env);

  // Gets a list of all tab group IDs.
  std::vector<base::Token> GetAllTabGroupIds(JNIEnv* env);

 private:
  // Returns a safe index for adding or moving a single tab without it changing
  // state.
  size_t GetSafeIndex(const std::optional<size_t>& current_index,
                      size_t proposed_index,
                      const std::optional<tab_groups::TabGroupId>& tab_group_id,
                      bool is_pinned) const;
  std::optional<tab_groups::TabGroupId> GetGroupIdAt(size_t index) const;
  const tab_groups::TabGroupVisualData* GetTabGroupVisualData(
      const base::Token& token_id,
      bool allow_detached) const;

  JavaObjectWeakGlobalRef java_object_;
  raw_ptr<Profile> profile_;

  // Always valid until destroyed.
  std::unique_ptr<tabs::TabStripCollection> tab_strip_collection_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_COLLECTION_TAB_MODEL_IMPL_H_
