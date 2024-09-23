// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/android/page_info_controller_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/android/page_info_client.h"
#include "components/page_info/core/features.h"
#include "components/page_info/page_info.h"
#include "components/page_info/page_info_ui.h"
#include "components/permissions/features.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "device/vr/buildflags/buildflags.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/page_info/android/jni_headers/PageInfoController_jni.h"

#if BUILDFLAG(ENABLE_VR)
#include "device/vr/public/cpp/features.h"
#endif

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

// static
static jlong JNI_PageInfoController_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry->IsInitialEntry())
    return 0;

  return reinterpret_cast<intptr_t>(
      new PageInfoControllerAndroid(env, obj, web_contents));
}

PageInfoControllerAndroid::PageInfoControllerAndroid(
    JNIEnv* env,
    jobject java_page_info_pop,
    content::WebContents* web_contents) {
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();

  url_ = nav_entry->GetURL();
  web_contents_ = web_contents;

  controller_jobject_.Reset(env, java_page_info_pop);

  page_info::PageInfoClient* page_info_client = page_info::GetPageInfoClient();
  DCHECK(page_info_client);
  presenter_ = std::make_unique<PageInfo>(
      page_info_client->CreatePageInfoDelegate(web_contents), web_contents,
      nav_entry->GetURL());
  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoControllerAndroid::~PageInfoControllerAndroid() = default;

void PageInfoControllerAndroid::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

void PageInfoControllerAndroid::RecordPageInfoAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint action) {
  presenter_->RecordPageInfoAction(
      static_cast<page_info::PageInfoAction>(action));
}

void PageInfoControllerAndroid::UpdatePermissions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  presenter_->UpdatePermissions();
}

void PageInfoControllerAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      GetSecurityDescription(identity_info);

  Java_PageInfoController_setSecurityDescription(
      env, controller_jobject_,
      ConvertUTF16ToJavaString(env, security_description->summary),
      ConvertUTF16ToJavaString(env, security_description->details));
}

void PageInfoControllerAndroid::SetPageFeatureInfo(
    const PageFeatureInfo& info) {
  NOTIMPLEMENTED();
}

void PageInfoControllerAndroid::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Exit without permissions if it is an internal page.
  if (PageInfo::IsFileOrInternalPage(url_)) {
    Java_PageInfoController_updatePermissionDisplay(env, controller_jobject_);
    return;
  }

  // On Android, we only want to display a subset of the available options in
  // a particular order, but only if their value is different from the
  // default. This order comes from https://crbug.com/610358.
  std::vector<ContentSettingsType> permissions_to_display;
  permissions_to_display.push_back(ContentSettingsType::GEOLOCATION);
  permissions_to_display.push_back(ContentSettingsType::MEDIASTREAM_CAMERA);
  permissions_to_display.push_back(ContentSettingsType::MEDIASTREAM_MIC);
  permissions_to_display.push_back(ContentSettingsType::NOTIFICATIONS);
  permissions_to_display.push_back(ContentSettingsType::IDLE_DETECTION);
  permissions_to_display.push_back(ContentSettingsType::IMAGES);
  permissions_to_display.push_back(ContentSettingsType::JAVASCRIPT);
  permissions_to_display.push_back(ContentSettingsType::POPUPS);
  permissions_to_display.push_back(ContentSettingsType::ADS);
  permissions_to_display.push_back(
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER);
  permissions_to_display.push_back(ContentSettingsType::SOUND);
  if (base::FeatureList::IsEnabled(features::kWebNfc))
    permissions_to_display.push_back(ContentSettingsType::NFC);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
    permissions_to_display.push_back(ContentSettingsType::BLUETOOTH_SCANNING);
  permissions_to_display.push_back(ContentSettingsType::VR);
  permissions_to_display.push_back(ContentSettingsType::AR);
#if BUILDFLAG(ENABLE_VR)
  if (device::features::IsHandTrackingEnabled()) {
    permissions_to_display.push_back(ContentSettingsType::HAND_TRACKING);
  }
#endif
  if (base::FeatureList::IsEnabled(features::kFedCm)) {
    permissions_to_display.push_back(
        ContentSettingsType::FEDERATED_IDENTITY_API);
  }
    permissions_to_display.push_back(ContentSettingsType::STORAGE_ACCESS);

  std::map<ContentSettingsType, ContentSetting>
      user_specified_settings_to_display;

  for (const auto& permission : permission_info_list) {
    if (base::Contains(permissions_to_display, permission.type)) {
      std::optional<ContentSetting> setting_to_display =
          GetSettingToDisplay(permission);
      if (setting_to_display) {
        user_specified_settings_to_display[permission.type] =
            *setting_to_display;
      }
    }
  }

  for (const auto& permission : permissions_to_display) {
    if (base::Contains(user_specified_settings_to_display, permission)) {
      std::u16string setting_title =
          PageInfoUI::PermissionTypeToUIString(permission);
      std::u16string setting_title_mid_sentence =
          PageInfoUI::PermissionTypeToUIStringMidSentence(permission);

      Java_PageInfoController_addPermissionSection(
          env, controller_jobject_,
          ConvertUTF16ToJavaString(env, setting_title),
          ConvertUTF16ToJavaString(env, setting_title_mid_sentence),
          static_cast<jint>(permission),
          static_cast<jint>(user_specified_settings_to_display[permission]));
    }
  }

  for (const auto& chosen_object : chosen_object_info_list) {
    std::u16string object_title =
        presenter_->GetChooserContextFromUIInfo(*chosen_object->ui_info)
            ->GetObjectDisplayName(chosen_object->chooser_object->value);

    Java_PageInfoController_addPermissionSection(
        env, controller_jobject_, ConvertUTF16ToJavaString(env, object_title),
        ConvertUTF16ToJavaString(env, object_title),
        static_cast<jint>(chosen_object->ui_info->content_settings_type),
        static_cast<jint>(CONTENT_SETTING_ALLOW));
  }

  Java_PageInfoController_updatePermissionDisplay(env, controller_jobject_);
}

std::optional<ContentSetting> PageInfoControllerAndroid::GetSettingToDisplay(
    const PageInfo::PermissionInfo& permission) {
  // All permissions should be displayed if they are non-default.
  if (permission.setting != CONTENT_SETTING_DEFAULT &&
      permission.setting != permission.default_setting) {
    return permission.setting;
  }

  // Handle exceptions for permissions which need to be displayed even if they
  // are set to the default.
  if (permission.type == ContentSettingsType::ADS) {
    // The subresource filter permission should always display the default
    // setting if it is showing up in Page Info. Logic for whether the
    // setting should show up in Page Info is in ShouldShowPermission in
    // page_info.cc.
    return permission.default_setting;
  } else if (permission.type == ContentSettingsType::JAVASCRIPT) {
    // The javascript content setting should show up if it is blocked globally
    // to give users an easy way to create exceptions.
    return permission.default_setting;
  } else if (permission.type == ContentSettingsType::SOUND) {
    // The sound content setting should always show up when the tab has played
    // audio since last navigation.
    if (web_contents_->WasEverAudible())
      return permission.default_setting;
  }

  // TODO(crbug.com/40129299): Also return permissions that are non
  // factory-default after we add the functionality to populate the permissions
  // subpage directly from the permissions returned from this controller.

  return std::nullopt;
}

void PageInfoControllerAndroid::SetAdPersonalizationInfo(
    const AdPersonalizationInfo& info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> topic_names;
  for (const auto& topic : info.accessed_topics) {
    topic_names.push_back(topic.GetLocalizedRepresentation());
  }
  Java_PageInfoController_setAdPersonalizationInfo(
      env, controller_jobject_, info.has_joined_user_to_interest_group,
      base::android::ToJavaArrayOfStrings(env, topic_names));
}
