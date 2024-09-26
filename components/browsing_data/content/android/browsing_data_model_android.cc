// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/android/browsing_data_model_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/functional/bind.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browsing_data/content/android/jni_headers/BrowsingDataModel_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

BrowsingDataModelAndroid::BrowsingDataModelAndroid(
    std::unique_ptr<BrowsingDataModel> model)
    : browsing_data_model_(std::move(model)) {}

BrowsingDataModelAndroid::~BrowsingDataModelAndroid() = default;

ScopedJavaLocalRef<jobject> BrowsingDataModelAndroid::GetBrowsingDataInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& map,
    bool fetch_important) {
  std::map<url::Origin, std::pair<uint64_t, uint64_t>> origin_to_data_map;

  for (const auto& [owner, key, details] : *browsing_data_model_) {
    const auto origin = BrowsingDataModel::GetOriginForDataKey(key.get());
    origin_to_data_map[origin].first += details->cookie_count;
    origin_to_data_map[origin].second += details->storage_size;
  }

  std::vector<std::pair<url::Origin, bool>> important_notations(
      origin_to_data_map.size());

  content::BrowserContext* browser_context =
      content::BrowserContextFromJavaHandle(jbrowser_context_handle);
  base::ranges::transform(origin_to_data_map, important_notations.begin(),
                          [](const auto& key_value) {
                            return std::make_pair(key_value.first, false);
                          });
  if (fetch_important) {
    permissions::PermissionsClient::Get()->AreSitesImportant(
        browser_context, &important_notations);
  }
  int i = 0;
  for (const auto& [origin, data] : origin_to_data_map) {
    Java_BrowsingDataModel_insertBrowsingDataInfoIntoMap(
        env, map, origin.ToJavaObject(env), /*cookieCount=*/data.first,
        /*storageSize=*/data.second,
        /*importantDomain=*/
        fetch_important ? important_notations[i++].second : false);
  }

  return ScopedJavaLocalRef<jobject>(map);
}

void BrowsingDataModelAndroid::RemoveBrowsingData(
    JNIEnv* env,
    const jstring host,
    const JavaParamRef<jobject>& java_callback) {
  browsing_data_model_->RemoveBrowsingData(
      ConvertJavaStringToUTF8(env, host),
      base::BindOnce(&base::android::RunRunnableAndroid,
                     ScopedJavaGlobalRef<jobject>(java_callback)));
}

void BrowsingDataModelAndroid::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}
