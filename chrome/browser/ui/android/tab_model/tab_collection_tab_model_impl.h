// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_COLLECTION_TAB_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_COLLECTION_TAB_MODEL_IMPL_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"

class Profile;

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
  int GetIndexOfTabRecursive(
      JNIEnv* env,
      const jni_zero::JavaParamRef<jobject>& j_tab_android) const;

  // Recurses until reaching the given index. Returns null if not found.
  base::android::ScopedJavaLocalRef<jobject> GetTabAtIndexRecursive(
      JNIEnv* env,
      size_t index) const;

 private:
  JavaObjectWeakGlobalRef java_object_;
  raw_ptr<Profile> profile_;

  // Always valid until destroyed.
  std::unique_ptr<tabs::TabStripCollection> tab_strip_collection_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_COLLECTION_TAB_MODEL_IMPL_H_
