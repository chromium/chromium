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
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
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

// Converts the wrapper class TabInterfaceAndroid* to a TabAndroid*. This will
// crash if the `tab_interface` has outlived the TabAndroid*.
TabAndroid* ToTabAndroid(TabInterface* tab_interface) {
  auto weak_tab_android =
      static_cast<TabInterfaceAndroid*>(tab_interface)->GetWeakPtr();
  CHECK(weak_tab_android);
  return static_cast<TabAndroid*>(weak_tab_android.get());
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
    if (ToTabAndroid(tab_in_collection) == tab_android) {
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
  return ToTabAndroid(tab);
}

int TabCollectionTabModelImpl::MoveTabRecursive(
    JNIEnv* env,
    size_t current_index,
    size_t new_index,
    const std::optional<base::Token>& token,
    bool new_is_pinned) {
  std::optional<TabGroupId> new_tab_group_id =
      tab_groups::TabGroupId::FromOptionalToken(token);
  new_index =
      GetSafeIndex(current_index, new_index, new_tab_group_id, new_is_pinned);

  tab_strip_collection_->MoveTabRecursive(current_index, new_index,
                                          new_tab_group_id, new_is_pinned);
  return base::checked_cast<int>(new_index);
}

void TabCollectionTabModelImpl::AddTabRecursive(
    JNIEnv* env,
    TabAndroid* tab_android,
    size_t index,
    const std::optional<base::Token>& token,
    bool is_pinned) {
  CHECK(tab_android);

  std::optional<TabGroupId> tab_group_id =
      tab_groups::TabGroupId::FromOptionalToken(token);

  index = GetSafeIndex(std::nullopt, index, tab_group_id, is_pinned);

  auto tab_interface_android = ToTabInterface(tab_android);
  tab_strip_collection_->AddTabRecursive(std::move(tab_interface_android),
                                         index, tab_group_id, is_pinned);
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
  std::optional<TabGroupId> tab_group_id =
      tab_groups::TabGroupId::FromRawToken(token);
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(*tab_group_id);

  std::vector<TabAndroid*> tabs;
  if (!group_collection) {
    return tabs;
  }

  tabs.reserve(group_collection->TabCountRecursive());
  for (TabInterface* group_tab : *group_collection) {
    tabs.push_back(ToTabAndroid(group_tab));
  }
  return tabs;
}

int TabCollectionTabModelImpl::MoveTabGroupTo(JNIEnv* env,
                                              const base::Token& token,
                                              int to_index) {
  TabGroupId tab_group_id = TabGroupId::FromRawToken(token);
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(tab_group_id);
  CHECK(group_collection);
  gfx::Range range = group_collection->GetTabGroup()->ListTabs();
  // TODO(crbug.com/429145597): Reusing GetSafeIndex here might not work for
  // groups with multiple tabs.

  // Don't pass the `tab_group_id` since we don't want to constrain the index
  // range to that of the group. Instead we are moving the entirety of the
  // group to any valid position that an ungrouped tab could be moved to.
  to_index = GetSafeIndex(range.start(), to_index,
                          /*tab_group_id=*/std::nullopt,
                          /*is_pinned=*/false);
  tab_strip_collection_->MoveTabGroupTo(tab_group_id, to_index);
  return base::checked_cast<int>(to_index);
}

void TabCollectionTabModelImpl::UpdateTabGroupVisualData(
    JNIEnv* env,
    const base::Token& tab_group_id,
    const std::optional<std::u16string>& tab_group_title,
    const std::optional<jint>& j_color_id,
    const std::optional<bool>& is_collapsed) {
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(
          TabGroupId::FromRawToken(tab_group_id));
  CHECK(group_collection);
  TabGroup* group = group_collection->GetTabGroup();
  CHECK(group);
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
  const TabGroupVisualData* visual_data =
      GetTabGroupVisualData(tab_group_id, /*allow_detached=*/true);
  return visual_data->title();
}

jint TabCollectionTabModelImpl::GetTabGroupColor(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data =
      GetTabGroupVisualData(tab_group_id, /*allow_detached=*/true);
  return static_cast<jint>(visual_data->color());
}

bool TabCollectionTabModelImpl::GetTabGroupCollapsed(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  const TabGroupVisualData* visual_data =
      GetTabGroupVisualData(tab_group_id, /*allow_detached=*/true);
  return visual_data->is_collapsed();
}

void TabCollectionTabModelImpl::CloseDetachedTabGroup(
    JNIEnv* env,
    const base::Token& tab_group_id) {
  tab_strip_collection_->CloseDetachedTabGroup(
      TabGroupId::FromRawToken(tab_group_id));
}

size_t TabCollectionTabModelImpl::GetSafeIndex(
    const std::optional<size_t>& current_index,
    size_t proposed_index,
    const std::optional<TabGroupId>& tab_group_id,
    bool is_pinned) const {
  bool is_move = current_index.has_value();
  size_t first_non_pinned_index =
      tab_strip_collection_->IndexOfFirstNonPinnedTab();
  if (is_move && *current_index < first_non_pinned_index) {
    // Moving a tab that is inside the pinned section should decrement the first
    // non-pinned index by one to either keep the pinned tabs together or move
    // to the new first non-pinned tab index after unpinning.
    first_non_pinned_index--;
  }

  if (is_pinned) {
    return std::min(proposed_index, first_non_pinned_index);
  }

  size_t tab_count = tab_strip_collection_->TabCountRecursive();
  if (is_move) {
    --tab_count;
  }
  size_t clamped_index =
      std::clamp(proposed_index, first_non_pinned_index, tab_count);
  if (tab_group_id) {
    TabGroupTabCollection* group_collection =
        tab_strip_collection_->GetTabGroupCollection(*tab_group_id);
    if (group_collection) {
      gfx::Range range = group_collection->GetTabGroup()->ListTabs();
      if (!range.is_empty()) {
        return std::clamp(proposed_index, range.start(), range.end());
      }
    }
  }

  // Always safe since these are the edges.
  if (clamped_index == first_non_pinned_index || clamped_index == tab_count) {
    return clamped_index;
  }

  std::optional<TabGroupId> group_at_index = GetGroupIdAt(clamped_index);
  if (group_at_index && group_at_index == GetGroupIdAt(clamped_index - 1)) {
    // Insertion will happen inside a tab group we need to push it out.
    TabGroupTabCollection* group_collection =
        tab_strip_collection_->GetTabGroupCollection(*group_at_index);
    gfx::Range range = group_collection->GetTabGroup()->ListTabs();
    // Push to the nearest boundary.
    if (clamped_index - range.start() < range.end() - clamped_index) {
      return range.start();
    } else {
      return range.end();
    }
  }

  return clamped_index;
}

std::optional<TabGroupId> TabCollectionTabModelImpl::GetGroupIdAt(
    size_t index) const {
  if (index < tab_strip_collection_->TabCountRecursive()) {
    return tab_strip_collection_->GetTabAtIndexRecursive(index)->GetGroup();
  } else {
    return std::nullopt;
  }
}

const TabGroupVisualData* TabCollectionTabModelImpl::GetTabGroupVisualData(
    const base::Token& token_id,
    bool allow_detached) const {
  TabGroupId tab_group_id = TabGroupId::FromRawToken(token_id);
  TabGroupTabCollection* group_collection =
      tab_strip_collection_->GetTabGroupCollection(tab_group_id);
  if (!group_collection && allow_detached) {
    group_collection = tab_strip_collection_->GetDetachedTabGroup(tab_group_id);
  }
  CHECK(group_collection);
  TabGroup* group = group_collection->GetTabGroup();
  CHECK(group);
  const TabGroupVisualData* visual_data = group->visual_data();
  CHECK(visual_data);
  return visual_data;
}

std::vector<TabAndroid*> TabCollectionTabModelImpl::GetAllTabs(JNIEnv* env) {
  std::vector<TabAndroid*> tabs;
  tabs.reserve(tab_strip_collection_->TabCountRecursive());

  for (TabInterface* tab_in_collection : *tab_strip_collection_) {
    tabs.push_back(ToTabAndroid(tab_in_collection));
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

static jlong JNI_TabCollectionTabModelImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_java_object,
    Profile* profile) {
  TabCollectionTabModelImpl* tab_collection_tab_model_impl =
      new TabCollectionTabModelImpl(env, j_java_object, profile);
  return reinterpret_cast<intptr_t>(tab_collection_tab_model_impl);
}

}  // namespace tabs
