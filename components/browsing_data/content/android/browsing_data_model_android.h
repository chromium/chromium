// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browsing_data/content/browsing_data_model.h"

class BrowsingDataModelShared
    : public base::RefCounted<BrowsingDataModelShared> {
 public:
  explicit BrowsingDataModelShared(std::unique_ptr<BrowsingDataModel> model);

  BrowsingDataModelShared(const BrowsingDataModelShared&) = delete;
  BrowsingDataModelShared& operator=(const BrowsingDataModelShared&) = delete;

  BrowsingDataModel* model() const { return browsing_data_model_.get(); }

  base::WeakPtr<BrowsingDataModelShared> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class base::RefCounted<BrowsingDataModelShared>;
  ~BrowsingDataModelShared();

  std::unique_ptr<BrowsingDataModel> browsing_data_model_;
  base::WeakPtrFactory<BrowsingDataModelShared> weak_ptr_factory_{this};
};

class BrowsingDataModelAndroid {
 public:
  static base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      JNIEnv* env,
      scoped_refptr<BrowsingDataModelShared> shared);

  explicit BrowsingDataModelAndroid(
      scoped_refptr<BrowsingDataModelShared> shared);
  ~BrowsingDataModelAndroid();

  BrowsingDataModelAndroid(const BrowsingDataModelAndroid&) = delete;
  BrowsingDataModelAndroid& operator=(const BrowsingDataModelAndroid&) = delete;

  base::android::ScopedJavaLocalRef<jobject> GetBrowsingDataInfo(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jbrowser_context_handle,
      const base::android::JavaRef<jobject>& map,
      bool fetch_important);

  void RemoveBrowsingData(JNIEnv* env,
                          const std::string& host,
                          base::OnceClosure&& java_callback);

 private:
  scoped_refptr<BrowsingDataModelShared> shared_;
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_ANDROID_BROWSING_DATA_MODEL_ANDROID_H_
