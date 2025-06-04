// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_collection_tab_model_impl.h"

#include <utility>

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_strip_collection.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
// This JNI header is generated from TabCollectionTabModelImpl.java.
#include "chrome/android/chrome_jni_headers/TabCollectionTabModelImpl_jni.h"

namespace tabs {

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
