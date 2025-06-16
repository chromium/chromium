// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_collection_tab_model_impl.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from TabCollectionTabModelImpl.java.
#include "chrome/android/chrome_jni_headers/TabCollectionTabModelImpl_jni.h"

namespace tabs {

namespace {
constexpr int kInvalidTabIndex = -1;
}  // namespace

TabCollectionTabModelImpl::TabCollectionTabModelImpl(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& java_object,
    Profile* profile)
    : java_object_(env, java_object),
      profile_(profile),
      tab_strip_collection_(std::make_unique<tabs::TabStripCollection>()) {}

TabCollectionTabModelImpl::~TabCollectionTabModelImpl() = default;

void TabCollectionTabModelImpl::Destroy(JNIEnv* env) {
  delete this;
}

int TabCollectionTabModelImpl::GetTabCountRecursive(JNIEnv* env) const {
  return base::checked_cast<int>(tab_strip_collection_->TabCountRecursive());
}

int TabCollectionTabModelImpl::GetIndexOfTabRecursive(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_tab_android) const {
  TabAndroid* target_tab = TabAndroid::GetNativeTab(env, j_tab_android);
  if (!target_tab) {
    return kInvalidTabIndex;
  }

  int current_index = 0;
  for (tabs::TabInterface* tab_in_collection : *tab_strip_collection_) {
    if (tab_in_collection == target_tab) {
      return current_index;
    }
    current_index++;
  }
  return kInvalidTabIndex;
}

static jlong JNI_TabCollectionTabModelImpl_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_java_object,
    const base::android::JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  tabs::TabCollectionTabModelImpl* tab_collection_tab_model_impl =
      new tabs::TabCollectionTabModelImpl(env, j_java_object, profile);
  return reinterpret_cast<intptr_t>(tab_collection_tab_model_impl);
}

}  // namespace tabs
