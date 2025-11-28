// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_collection_tab_model_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_android_conversions.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/android/jni_conversion.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "components/tabs/public/unpinned_tab_collection.h"
#include "tab_collection_tab_model_impl.h"
#include "ui/gfx/range/range.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from TabCollectionTabModelImpl.java.
#include "chrome/android/chrome_jni_headers/TabCollectionTabModelImpl_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::TokenAndroid;
using tab_groups::TabGroupColorId;
using tab_groups::TabGroupId;
using tab_groups::TabGroupVisualData;

namespace tabs {

namespace {

constexpr int kInvalidTabIndex = -1;

// Converts the `tab_android` to a `unique_ptr<TabInterface>`. Under the hood we
// use a wrapper class `TabInterfaceAndroid` which takes a weak ptr to
// `TabAndroid` to avoid memory management issues.
std::unique_ptr<TabInterface> ToTabInterface(TabAndroid* tab_android) {
  return std::make_unique<TabInterfaceAndroid>(tab_android);
}

// When moving a tab from a lower index to a higher index a value of 1 less
// should be used to account for the tab being removed from the list before it
// is re-inserted.
size_t ClampIfMovingToHigherIndex(const std::optional<size_t>& current_index,
                                  size_t new_index) {
  return (new_index != 0 && current_index && *current_index < new_index)
             ? new_index - 1
             : new_index;
}

}  // namespace

TabCollectionTabModelImpl::TabCollectionTabModelImpl(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& java_object,
    Profile* profile)
    : java_object_(env, java_object),
      profile_(profile),
      tab_strip_collection_(std::make_unique<TabStripCollection>()) {}

TabCollectionTabModelImpl::~TabCollectionTabModelImpl() = default;

void TabCollectionTabModelImpl::Destroy(JNIEnv* env) {
  delete this;
}

int TabCollectionTabModelImpl::GetTabCountRecursive(JNIEnv* env) const {
  return base::checked_cast<int>(tab_strip_collection_->TabCountRecursive());
}

int TabCollectionTabModelImpl::GetIndexOfTabRecursive(
    JNIEnv* env,
    TabAndroid* tab_android) const {
  if (!tab_android) {
    return kInvalidTabIndex;
  }

  int current_index = 0;
  for (TabInterface* tab_in_collection : *tab_strip_collection_) {
    if (ToTabAndroidChecked(tab_in_collection) == tab_android) {
      return current_index;
    }
    current_index++;
  }
  return kInvalidTabIndex;
}

TabAndroid* TabCollectionTabModelImpl::GetTabAtIndexRecursive(
    JNIEnv* env,
    size_t index) const {
  if (index >= tab_strip_collection_->TabCountRecursive()) {
    return nullptr;
  }
  TabInterface* tab = tab_strip_collection_->GetTabAtIndexRecursive(index);
  return ToTabAndroidOrNull(tab);
}

int TabCollectionTabModelImpl::MoveTabRecursive(
    JNIEnv* env,
    size_t current_index,
    size_t new_index,
    const std::optional<base::Token>& token,
    bool new_is_pinned) {
  std::optional<TabGroupId> new_tab_group_id =
      tab_groups::TabGroupId::FromOptionalToken(token);
  new_index = GetSafeIndex(/*is_tab_group=*/false, current_index, new_index,
                           new_tab_group_id, new_is_pinned);

  tab_strip_collection_->MoveTabRecursive(current_index, new_index,
                                          new_tab_group_id, new_is_pinned);
  return base::checked_cast<int>(new_index);
}

int TabCollectionTabModelImpl::AddTabRecursive(
    JNIEnv* env,
    TabAndroid* tab_android,
    size_t index,
    const std::optional<base::Token>& token,
    bool is_attaching_group,
    bool is_pinned) {
  CHECK(tab_android);

  std::optional<TabGroupId> tab_group_id =
      tab_groups::TabGroupId::FromOptionalToken(token);

  index = GetSafeIndex(/*is_tab_group=*/false, /*current_index=*/std::nullopt,
                       index, tab_group_id, is_pinned);

  auto tab_interface_android = ToTabInterface(tab_android);

  // When the tab is attaching a detached group we first add the tab to the
  // collection and then move the tab to the group.
  tab_strip_collection_->AddTabRecursive(
      std::move(tab_interface_android), index,
      is_attaching_group ? std::nullopt : tab_group_id, is_pinned);

  if (is_attaching_group) {
    tab_strip_collection_->MoveTabRecursive(index, index, *tab_group_id,
                                            is_pinned);
  }
  return base::checked_cast<int>(index);
}

void TabCollectionTabModelImpl::RemoveTabRecursive(JNIEnv* env,
                                                   TabAndroid* tab) {
  int index = GetIndexOfTabRecursive(env, tab);
  CHECK_NE(index, kInvalidTabIndex);
  tab_strip_collection_->RemoveTabAtIndexRecursive(index);
}

void TabCollectionTabModelImpl::CreateTabGroup(
    JNIEnv* env,
    const base::Token& tab_group_id,
    const std::u16string& tab_group_title,
    jint j_color_id,
    bool is_collapsed) {
  TabGroupAndroid::Factory factory(profile_);
  std::unique_ptr<TabGroupTabCollection> group_collection =
      std::make_unique<TabGroupTabCollection>(
          factory, TabGroupId::FromRawToken(tab_group_id),
          TabGroupVisualData(tab_group_title,
                             static_cast<TabGroupColorId>(j_color_id),
                             is_collapsed));
  tab_strip_collection_->CreateTabGroup(std::move(group_collection));
}

std::vector<TabAndroid*> TabCollectionTabModelImpl::GetTabsInGroup(
    JNIEnv* env,
    const base::Token& token) {
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(
          TabGroupId::FromRawToken(token));

  std::vector<TabAndroid*> tabs;
  if (!group_collection) {
    return tabs;
  }

  tabs.reserve(group_collection->TabCountRecursive());
  for (TabInterface* group_tab : *group_collection) {
    tabs.push_back(ToTabAndroidChecked(group_tab));
  }
  return tabs;
}

int TabCollectionTabModelImpl::GetTabCountForGroup(JNIEnv* env,
                                                   const base::Token& token) {
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(
          TabGroupId::FromRawToken(token));

  if (!group_collection) {
    return 0;
  }

  return group_collection->TabCountRecursive();
}

bool TabCollectionTabModelImpl::TabGroupExists(JNIEnv* env,
                                               const base::Token& token) {
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(
          TabGroupId::FromRawToken(token));
  return group_collection;
}

int TabCollectionTabModelImpl::GetIndividualTabAndGroupCount(JNIEnv* env) {
  // The direct child count of the pinned and unpinned collections will include
  // all individual tabs and tab groups.
  return tab_strip_collection_->unpinned_collection()->ChildCount() +
         tab_strip_collection_->pinned_collection()->ChildCount();
}

int TabCollectionTabModelImpl::GetTabGroupCount(JNIEnv* env) {
  return tab_strip_collection_->GetAllTabGroupIds().size();
}

int TabCollectionTabModelImpl::GetIndexOfTabInGroup(JNIEnv* env,
                                                    TabAndroid* tab_android,
                                                    const base::Token& token) {
  CHECK(tab_android);

  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(
          TabGroupId::FromRawToken(token));

  if (!group_collection) {
    return kInvalidTabIndex;
  }

  int index = 0;
  for (TabInterface* group_tab : *group_collection) {
    if (ToTabAndroidChecked(group_tab) == tab_android) {
      return index;
    }
    index++;
  }

  return kInvalidTabIndex;
}

int TabCollectionTabModelImpl::MoveTabGroupTo(JNIEnv* env,
                                              const base::Token& token,
                                              int to_index) {
  TabGroupId tab_group_id = TabGroupId::FromRawToken(token);
  TabGroup* group = GetTabGroupChecked(tab_group_id);
  gfx::Range range = group->ListTabs();
  to_index =
      GetSafeIndex(/*is_tab_group=*/true, range.start(), to_index, tab_group_id,
                   /*is_pinned=*/false);
  // When moving to a higher index the implementation will first remove the tab
  // group before adding the tab group. This means the destination index needs
  // to account for the size of the group. To do this we subtract the number of
  // tabs in the group from the `to_index`. Note that GetSafeIndex() already
  // subtracts one when moving to a higher index so we subtract 1 less.
  if (to_index >= base::checked_cast<int>(range.end() - 1U)) {
    to_index -= range.length() - 1;
    CHECK_GE(to_index, 0);
  }

  std::vector<int> tab_indices;
  tab_indices.reserve(range.length());
  for (size_t i = range.start(); i < range.end(); ++i) {
    tab_indices.push_back(base::checked_cast<int>(i));
  }

  const std::set<tabs::TabCollection::Type> kRetainCollectionTypes =
      std::set<tabs::TabCollection::Type>(
          {tabs::TabCollection::Type::SPLIT, tabs::TabCollection::Type::GROUP});

  tab_strip_collection_->MoveTabsRecursive(
      tab_indices, static_cast<size_t>(to_index),
      /*new_group_id=*/std::nullopt,
      /*new_pinned_state=*/false, kRetainCollectionTypes);

  return base::checked_cast<int>(to_index);
}

void TabCollectionTabModelImpl::UpdateTabGroupVisualData(
    JNIEnv* env,
    const base::Token& tab_group_id,
    const std::optional<std::u16string>& tab_group_title,
    const std::optional<jint>& j_color_id,
    const std::optional<bool>& is_collapsed) {
  TabGroup* group = GetTabGroupChecked(TabGroupId::FromRawToken(tab_group_id));
  const TabGroupVisualData* old_visual_data = group->visual_data();
  CHECK(old_visual_data);

  TabGroupVisualData new_visual_data(
      tab_group_title.value_or(old_visual_data->title()),
      j_color_id.has_value() ? static_cast<TabGroupColorId>(j_color_id.value())
                             : old_visual_data->color(),
      is_collapsed.value_or(old_visual_data->is_collapsed()));
  group->SetVisualData(new_visual_data);
}

std::u16string TabCollectionTabModelImpl::GetTabGroupTitle(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data = GetTabGroupVisualDataChecked(
      TabGroupId::FromRawToken(tab_group_id), /*allow_detached=*/true);
  return visual_data->title();
}

jint TabCollectionTabModelImpl::GetTabGroupColor(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data = GetTabGroupVisualDataChecked(
      TabGroupId::FromRawToken(tab_group_id), /*allow_detached=*/true);
  return static_cast<jint>(visual_data->color());
}

bool TabCollectionTabModelImpl::GetTabGroupCollapsed(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data = GetTabGroupVisualDataChecked(
      TabGroupId::FromRawToken(tab_group_id), /*allow_detached=*/true);
  return visual_data->is_collapsed();
}

bool TabCollectionTabModelImpl::DetachedTabGroupExists(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  TabGroupId group_id = TabGroupId::FromRawToken(tab_group_id);
  TabGroupTabCollection* detached_group =
      tab_strip_collection_->GetDetachedTabGroup(group_id);
  return detached_group != nullptr;
}

void TabCollectionTabModelImpl::CloseDetachedTabGroup(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  TabGroupId group_id = TabGroupId::FromRawToken(tab_group_id);
  TabGroupTabCollection* detached_group =
      tab_strip_collection_->GetDetachedTabGroup(group_id);
  if (!detached_group) {
    CHECK(!tab_strip_collection_->GetTabGroupCollection(group_id))
        << "Tried to close an attached tab group.";
    LOG(WARNING) << "Detached tab group already closed.";
    return;
  }
  tab_strip_collection_->CloseDetachedTabGroup(group_id);
}

std::vector<TabAndroid*> TabCollectionTabModelImpl::GetAllTabs(JNIEnv* env) {
  std::vector<TabAndroid*> tabs;
  tabs.reserve(tab_strip_collection_->TabCountRecursive());

  for (TabInterface* tab_in_collection : *tab_strip_collection_) {
    TabAndroid* tab = ToTabAndroidOrNull(tab_in_collection);
    if (!tab) {
      continue;
    }
    tabs.push_back(tab);
  }
  return tabs;
}

std::vector<base::Token> TabCollectionTabModelImpl::GetAllTabGroupIds(
    JNIEnv* env) {
  std::vector<TabGroupId> group_ids =
      tab_strip_collection_->GetAllTabGroupIds();

  std::vector<base::Token> tokens;
  tokens.reserve(group_ids.size());
  for (const TabGroupId& group_id : group_ids) {
    tokens.push_back(group_id.token());
  }
  return tokens;
}

std::vector<TabAndroid*> TabCollectionTabModelImpl::GetRepresentativeTabList(
    JNIEnv* env) {
  std::vector<TabAndroid*> tabs;
  tabs.reserve(tab_strip_collection_->pinned_collection()->ChildCount() +
               tab_strip_collection_->unpinned_collection()->ChildCount());

  std::optional<TabGroupId> current_group_id = std::nullopt;
  for (TabInterface* tab : *tab_strip_collection_) {
    std::optional<TabGroupId> tab_group_id = tab->GetGroup();
    if (!tab_group_id) {
      current_group_id = std::nullopt;
      tabs.push_back(ToTabAndroidChecked(tab));
    } else if (current_group_id != tab_group_id) {
      current_group_id = tab_group_id;
      TabGroupAndroid* group =
          static_cast<TabGroupAndroid*>(GetTabGroupChecked(*tab_group_id));

      std::optional<TabHandle> last_shown_tab = group->last_shown_tab();
      // By the time a tab group is used in GetRepresentativeTabList it should
      // have a valid `last_shown_tab`. The only time this should be empty is
      // either while the tab group is detached or during the synchronous
      // process of attaching the group. During neither of these times is this
      // method expected to be called.
      CHECK(last_shown_tab);
      TabAndroid* tab_android = TabAndroid::FromTabHandle(*last_shown_tab);
      CHECK(tab_android);
      tabs.push_back(tab_android);
    }
  }
  return tabs;
}

void TabCollectionTabModelImpl::SetLastShownTabForGroup(
    JNIEnv* env,
    const base::Token& group_id,
    TabAndroid* tab_android) {
  TabGroupAndroid* group = static_cast<TabGroupAndroid*>(GetTabGroupChecked(
      TabGroupId::FromRawToken(group_id), /*allow_detached=*/true));
  if (tab_android) {
    group->set_last_shown_tab(tab_android->GetHandle());
  } else {
    group->set_last_shown_tab(std::nullopt);
  }
}

TabAndroid* TabCollectionTabModelImpl::GetLastShownTabForGroup(
    JNIEnv* env,
    const base::Token& group_id) {
  TabGroupAndroid* group = static_cast<TabGroupAndroid*>(
      GetTabGroupChecked(TabGroupId::FromRawToken(group_id),
                         /*allow_detached=*/true));
  auto handle = group->last_shown_tab();
  if (!handle) {
    return nullptr;
  }
  return TabAndroid::FromTabHandle(*handle);
}

int TabCollectionTabModelImpl::GetIndexOfFirstNonPinnedTab(JNIEnv* env) {
  return tab_strip_collection_->IndexOfFirstNonPinnedTab();
}

TabStripCollection* TabCollectionTabModelImpl::GetTabStripCollection(
    JNIEnv* env) {
  return tab_strip_collection_.get();
}

// Private methods:

size_t TabCollectionTabModelImpl::GetSafeIndex(
    bool is_tab_group,
    const std::optional<size_t>& current_index,
    size_t proposed_index,
    const std::optional<TabGroupId>& tab_group_id,
    bool is_pinned) const {
  size_t first_non_pinned_index = ClampIfMovingToHigherIndex(
      current_index, tab_strip_collection_->IndexOfFirstNonPinnedTab());
  if (is_pinned) {
    return std::min(proposed_index, first_non_pinned_index);
  }

  size_t total_tabs = ClampIfMovingToHigherIndex(
      current_index, tab_strip_collection_->TabCountRecursive());
  size_t clamped_index =
      std::clamp(proposed_index, first_non_pinned_index, total_tabs);

  // If a tab is part of a group, it cannot be moved out of the group.
  if (!is_tab_group && tab_group_id) {
    TabGroupTabCollection* group_collection =
        tab_strip_collection_->GetTabGroupCollection(*tab_group_id);
    if (group_collection) {
      gfx::Range range = group_collection->GetTabGroup()->ListTabs();
      if (!range.is_empty()) {
        return std::clamp(
            proposed_index,
            ClampIfMovingToHigherIndex(current_index, range.start()),
            ClampIfMovingToHigherIndex(current_index, range.end()));
      }
    }
  }

  // Early exit if the index is one of the edges, as it is a safe index.
  if (clamped_index == first_non_pinned_index || clamped_index == total_tabs) {
    return clamped_index;
  }

  // If the destination is inside a group, the tab should be moved to be
  // adjacent to the group.
  std::optional<TabGroupId> group_at_index = GetGroupIdAt(clamped_index);
  if (group_at_index) {
    // When moving a tab group to be within its own range this should no-op.
    if (is_tab_group && group_at_index == tab_group_id) {
      return current_index.value_or(clamped_index);
    }

    TabGroupTabCollection* destination_group_collection =
        tab_strip_collection_->GetTabGroupCollection(*group_at_index);
    gfx::Range destination_range =
        destination_group_collection->GetTabGroup()->ListTabs();

    // If a tab is being moved to a location that is a group of size 1, the move
    // should be allowed.
    if (destination_range.length() == 1) {
      return clamped_index;
    }

    // If the tab is otherwise inside a group we need to push the move outside
    // the group.
    const size_t front_delta = clamped_index - destination_range.start();
    const size_t back_delta = (destination_range.end() - 1) - clamped_index;

    // Check which side of the group the tab is being moved from. This is used
    // to determine which side of the group the tab should be moved to.
    const bool is_moving_from_left =
        current_index && *current_index < destination_range.start();

    // Push the tab to be in front of the group if the front delta is smaller or
    // the deltas are equal and the tab is being moved from in front of the
    // group.
    const bool keep_in_front =
        front_delta < back_delta ||
        (front_delta == back_delta && is_moving_from_left);

    if (keep_in_front) {
      return ClampIfMovingToHigherIndex(current_index,
                                        destination_range.start());
    } else {
      return ClampIfMovingToHigherIndex(current_index, destination_range.end());
    }
  }

  return clamped_index;
}

std::optional<TabGroupId> TabCollectionTabModelImpl::GetGroupIdAt(
    size_t index) const {
  if (index < tab_strip_collection_->TabCountRecursive()) {
    TabInterface* tab = tab_strip_collection_->GetTabAtIndexRecursive(index);
    if (!tab) {
      return std::nullopt;
    }
    return tab->GetGroup();
  } else {
    return std::nullopt;
  }
}

TabGroupTabCollection* TabCollectionTabModelImpl::GetTabGroupCollectionChecked(
    const TabGroupId& tab_group_id,
    bool allow_detached) const {
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(tab_group_id);
  if (!group_collection && allow_detached) {
    group_collection = tab_strip_collection_->GetDetachedTabGroup(tab_group_id);
  }
  CHECK(group_collection);
  return group_collection;
}

TabGroup* TabCollectionTabModelImpl::GetTabGroupChecked(
    const TabGroupId& tab_group_id,
    bool allow_detached) const {
  TabGroupTabCollection* group_collection =
      GetTabGroupCollectionChecked(tab_group_id, allow_detached);
  TabGroup* group = group_collection->GetTabGroup();
  CHECK(group);
  return group;
}

const TabGroupVisualData*
TabCollectionTabModelImpl::GetTabGroupVisualDataChecked(
    const TabGroupId& tab_group_id,
    bool allow_detached) const {
  TabGroup* group = GetTabGroupChecked(tab_group_id, allow_detached);
  const TabGroupVisualData* visual_data = group->visual_data();
  CHECK(visual_data);
  return visual_data;
}

static jlong JNI_TabCollectionTabModelImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_java_object,
    Profile* profile) {
  TabCollectionTabModelImpl* tab_collection_tab_model_impl =
      new TabCollectionTabModelImpl(env, j_java_object, profile);
  return reinterpret_cast<intptr_t>(tab_collection_tab_model_impl);
}

}  // namespace tabs

DEFINE_JNI(TabCollectionTabModelImpl)
