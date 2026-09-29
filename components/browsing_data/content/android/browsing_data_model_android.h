// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/browsing_data/content/browsing_data_model.h"

class BrowsingDataModelAndroid
    : public base::RefCounted<BrowsingDataModelAndroid> {
 public:
  // Builds the C++ counter part of BrowsingDataModel.java and owns a unique
  // pointer to the browsing data model.
  explicit BrowsingDataModelAndroid(std::unique_ptr<BrowsingDataModel> model);

  BrowsingDataModelAndroid(const BrowsingDataModelAndroid&) = delete;
  BrowsingDataModelAndroid& operator=(const BrowsingDataModelAndroid&) = delete;
  base::android::ScopedJavaLocalRef<jobject> GetBrowsingDataInfo(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jbrowser_context_handle,
      const base::android::JavaRef<jobject>& map,
      bool fetch_important);

  void RemoveBrowsingData(const std::string& host,
                          base::OnceClosure&& java_callback);

  // Releases a reference to the BrowsingDataModelAndroid object. This needs to
  // be called on the Java side when the object is not in use anymore.
  void ReleaseModel();

  base::WeakPtr<BrowsingDataModelAndroid> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class base::RefCounted<BrowsingDataModelAndroid>;
  ~BrowsingDataModelAndroid();
  std::unique_ptr<BrowsingDataModel> browsing_data_model_;
  base::WeakPtrFactory<BrowsingDataModelAndroid> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_
