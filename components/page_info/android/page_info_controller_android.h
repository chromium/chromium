// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_ANDROID_PAGE_INFO_CONTROLLER_ANDROID_H_
#define COMPONENTS_PAGE_INFO_ANDROID_PAGE_INFO_CONTROLLER_ANDROID_H_

#include <jni.h>

#include <memory>
#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/page_info/page_info_ui.h"

namespace content {
class WebContents;
}

// Android implementation of the page info UI.
class PageInfoControllerAndroid : public PageInfoUI {
 public:
  PageInfoControllerAndroid(JNIEnv* env,
                            jobject java_page_info,
                            content::WebContents* web_contents);

  PageInfoControllerAndroid(const PageInfoControllerAndroid&) = delete;
  PageInfoControllerAndroid& operator=(const PageInfoControllerAndroid&) =
      delete;

  ~PageInfoControllerAndroid() override;
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void RecordPageInfoAction(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            jint action);
  void UpdatePermissions(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  // PageInfoUI implementations.
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetPageFeatureInfo(const PageFeatureInfo& info) override;
  void SetAdPersonalizationInfo(
      const AdPersonalizationInfo& ad_personalization_info) override;

 private:
  // Returns an optional value which is set if this permission should be
  // displayed in Page Info. Most permissions will only be displayed if they are
  // set to some non-default value, but there are some permissions which require
  // customized behavior.
  std::optional<ContentSetting> GetSettingToDisplay(
      const PageInfo::PermissionInfo& permission);

  // The presenter that controlls the Page Info UI.
  std::unique_ptr<PageInfo> presenter_;

  // The java prompt implementation.
  base::android::ScopedJavaGlobalRef<jobject> controller_jobject_;

  GURL url_;

  raw_ptr<content::WebContents> web_contents_;
};

#endif  // COMPONENTS_PAGE_INFO_ANDROID_PAGE_INFO_CONTROLLER_ANDROID_H_
