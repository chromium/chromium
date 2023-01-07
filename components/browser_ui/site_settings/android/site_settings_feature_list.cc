// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/browser_ui/site_settings/android/features.h"
#include "components/browser_ui/site_settings/android/site_settings_jni_headers/SiteSettingsFeatureList_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace browser_ui {

namespace {

// Array of features exposed through the Java ContentFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &kSiteDataImprovements,
    &kRequestDesktopSiteExceptionsDowngrade,
};

// TODO(crbug.com/1060097): Remove this once a generalized FeatureList exists.
const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED() << "Queried feature not found in SiteSettingsFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

static jboolean JNI_SiteSettingsFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace browser_ui
