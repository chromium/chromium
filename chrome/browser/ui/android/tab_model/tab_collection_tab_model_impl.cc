// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_collection_tab_model_impl.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/token_android.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "chrome/browser/profiles/profile.h"
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
using tab_groups::TabGroupId;

namespace tabs {

namespace {

constexpr int kInvalidTabIndex = -1;

std::optional<TabGroupId> ToTabGroupId(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_tab_group_id) {
  if (!j_tab_group_id) {
    return std::nullopt;
  }
  return TabGroupId::FromRawToken(
      TokenAndroid::FromJavaToken(env, j_tab_group_id));
}

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
    const JavaParamRef<jobject>& j_tab_android) const {
  TabAndroid* target_tab = TabAndroid::GetNativeTab(env, j_tab_android);
  if (!target_tab) {
    return kInvalidTabIndex;
  }

  int current_index = 0;
  for (TabInterface* tab_in_collection : *tab_strip_collection_) {
    if (ToTabAndroid(tab_in_collection) == target_tab) {
      return current_index;
    }
    current_index++;
  }
  return kInvalidTabIndex;
}

ScopedJavaLocalRef<jobject> TabCollectionTabModelImpl::GetTabAtIndexRecursive(
    JNIEnv* env,
    size_t index) const {
  if (index >= tab_strip_collection_->TabCountRecursive()) {
    return ScopedJavaLocalRef<jobject>();
  }
  TabInterface* tab = tab_strip_collection_->GetTabAtIndexRecursive(index);
  TabAndroid* tab_android = ToTabAndroid(tab);
  return tab_android->GetJavaObject();
}

int TabCollectionTabModelImpl::MoveTabRecursive(
    JNIEnv* env,
    size_t current_index,
    size_t new_index,
    const JavaParamRef<jobject>& j_new_tab_group_id,
    bool new_is_pinned) {
  std::optional<TabGroupId> new_tab_group_id =
      ToTabGroupId(env, j_new_tab_group_id);
  new_index = GetSafeIndex(/*is_move=*/true, new_index, new_tab_group_id,
                           new_is_pinned);

  tab_strip_collection_->MoveTabRecursive(current_index, new_index,
                                          new_tab_group_id, new_is_pinned);
  return base::checked_cast<int>(new_index);
}

void TabCollectionTabModelImpl::AddTabRecursive(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_tab_android,
    size_t index,
    const JavaParamRef<jobject>& j_tab_group_id,
    bool is_pinned) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, j_tab_android);
  CHECK(tab_android);

  std::optional<TabGroupId> tab_group_id = ToTabGroupId(env, j_tab_group_id);

  index = GetSafeIndex(/*is_move=*/false, index, tab_group_id, is_pinned);

  auto tab_interface_android = ToTabInterface(tab_android);
  tab_strip_collection_->AddTabRecursive(std::move(tab_interface_android),
                                         index, tab_group_id, is_pinned);
}

size_t TabCollectionTabModelImpl::GetSafeIndex(
    bool is_move,
    size_t proposed_index,
    const std::optional<TabGroupId>& tab_group_id,
    bool is_pinned) const {
  size_t first_non_pinned_index =
      tab_strip_collection_->IndexOfFirstNonPinnedTab();
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

static jlong JNI_TabCollectionTabModelImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_java_object,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  TabCollectionTabModelImpl* tab_collection_tab_model_impl =
      new TabCollectionTabModelImpl(env, j_java_object, profile);
  return reinterpret_cast<intptr_t>(tab_collection_tab_model_impl);
}

}  // namespace tabs
