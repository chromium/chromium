// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/browsing_data/content/browsing_data_model.h"

class BrowsingDataModelAndroid {
 public:
  // Builds the C++ counter part of BrowsingDataModel.java and owns a unique
  // pointer to the browsing data model.
  explicit BrowsingDataModelAndroid(std::unique_ptr<BrowsingDataModel> model);

  BrowsingDataModelAndroid(const BrowsingDataModelAndroid&) = delete;
  BrowsingDataModelAndroid& operator=(const BrowsingDataModelAndroid&) = delete;

  ~BrowsingDataModelAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetBrowsingDataInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jbrowser_context_handle,
      const base::android::JavaParamRef<jobject>& map,
      bool fetch_important);

  void RemoveBrowsingData(
      JNIEnv* env,
      const jstring host,
      const base::android::JavaParamRef<jobject>& java_callback);

  // Destroys the BrowsingDataModelAndroid object. This needs to be called on
  // the java side when the object is not in use anymore.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  std::unique_ptr<BrowsingDataModel> browsing_data_model_;
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_
