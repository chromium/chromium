// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <algorithm>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "components/browser_ui/site_settings/android/site_settings_jni_headers/WebsitePreferenceBridge_jni.h"
#include "components/browser_ui/site_settings/android/storage_info_fetcher.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/uma_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/embedder_support/android/browser_context/browser_context_handle.h"
#include "components/permissions/chooser_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserContext;
using content::BrowserThread;

namespace {

const char kHttpPortSuffix[] = ":80";
const char kHttpsPortSuffix[] = ":443";

BrowserContext* unwrap(const JavaParamRef<jobject>& jbrowser_context_handle) {
  return browser_context::BrowserContextFromJavaHandle(jbrowser_context_handle);
}

HostContentSettingsMap* GetHostContentSettingsMap(
    BrowserContext* browser_context) {
  return permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
}

HostContentSettingsMap* GetHostContentSettingsMap(
    const JavaParamRef<jobject>& jbrowser_context_handle) {
  return GetHostContentSettingsMap(unwrap(jbrowser_context_handle));
}

// Reset the give permission for the DSE if the permission and origin are
// controlled by the DSE.
bool MaybeResetDSEPermission(BrowserContext* browser_context,
                             ContentSettingsType type,
                             const GURL& origin,
                             const GURL& embedder,
                             ContentSetting setting) {
  if (!embedder.is_empty() && embedder != origin)
    return false;

  if (setting != CONTENT_SETTING_DEFAULT)
    return false;

  return permissions::PermissionsClient::Get()
      ->ResetPermissionIfControlledByDse(browser_context, type,
                                         url::Origin::Create(origin));
}

ScopedJavaLocalRef<jstring> ConvertOriginToJavaString(
    JNIEnv* env,
    const std::string& origin) {
  // The string |jorigin| is used to group permissions together in the Site
  // Settings list. In order to group sites with the same origin, remove any
  // standard port from the end of the URL if it's present (i.e. remove :443
  // for HTTPS sites and :80 for HTTP sites).
  // TODO(mvanouwerkerk): Remove all this logic and take two passes through
  // HostContentSettingsMap: once to get all the 'interesting' hosts, and once
  // (on SingleWebsitePreferences) to find permission patterns which match
  // each of these hosts.
  if (base::StartsWith(origin, url::kHttpsScheme,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      base::EndsWith(origin, kHttpsPortSuffix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return ConvertUTF8ToJavaString(
        env, origin.substr(0, origin.size() - strlen(kHttpsPortSuffix)));
  } else if (base::StartsWith(origin, url::kHttpScheme,
                              base::CompareCase::INSENSITIVE_ASCII) &&
             base::EndsWith(origin, kHttpPortSuffix,
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return ConvertUTF8ToJavaString(
        env, origin.substr(0, origin.size() - strlen(kHttpPortSuffix)));
  } else {
    return ConvertUTF8ToJavaString(env, origin);
  }
}

typedef void (*InfoListInsertionFunction)(
    JNIEnv*,
    const base::android::JavaRef<jobject>&,
    const base::android::JavaRef<jstring>&,
    const base::android::JavaRef<jstring>&,
    jboolean);

void GetOrigins(JNIEnv* env,
                const JavaParamRef<jobject>& jbrowser_context_handle,
                ContentSettingsType content_type,
                InfoListInsertionFunction insertionFunc,
                const JavaRef<jobject>& list,
                jboolean managedOnly) {
  HostContentSettingsMap* content_settings_map =
      GetHostContentSettingsMap(jbrowser_context_handle);
  ContentSettingsForOneType all_settings;
  ContentSettingsForOneType embargo_settings;

  content_settings_map->GetSettingsForOneType(content_type, std::string(),
                                              &all_settings);
  content_settings_map->GetSettingsForOneType(
      ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA, std::string(),
      &embargo_settings);
  ContentSetting default_content_setting =
      content_settings_map->GetDefaultContentSetting(content_type, nullptr);

  // Use a vector since the overall number of origins should be small.
  std::vector<std::string> seen_origins;

  // Now add all origins that have a non-default setting to the list.
  for (const auto& settings_it : all_settings) {
    if (settings_it.GetContentSetting() == default_content_setting)
      continue;
    if (managedOnly &&
        HostContentSettingsMap::GetProviderTypeFromSource(settings_it.source) !=
            HostContentSettingsMap::ProviderType::POLICY_PROVIDER) {
      continue;
    }
    const std::string origin = settings_it.primary_pattern.ToString();
    const std::string embedder = settings_it.secondary_pattern.ToString();

    ScopedJavaLocalRef<jstring> jembedder;
    if (embedder != origin)
      jembedder = ConvertUTF8ToJavaString(env, embedder);

    seen_origins.push_back(origin);
    insertionFunc(env, list, ConvertOriginToJavaString(env, origin), jembedder,
                  /*is_embargoed=*/false);
  }

  // Add any origins which have a default content setting value (thus skipped
  // above), but have been automatically blocked for this permission type.
  // We use an empty embedder since embargo doesn't care about it.
  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          unwrap(jbrowser_context_handle));
  ScopedJavaLocalRef<jstring> jembedder;

  for (const auto& settings_it : embargo_settings) {
    const std::string origin = settings_it.primary_pattern.ToString();
    if (base::Contains(seen_origins, origin)) {
      // This origin has already been added to the list, so don't add it again.
      continue;
    }

    if (auto_blocker->GetEmbargoResult(GURL(origin), content_type)
            .content_setting == CONTENT_SETTING_BLOCK) {
      seen_origins.push_back(origin);
      insertionFunc(env, list, ConvertOriginToJavaString(env, origin),
                    jembedder, /*is_embargoed=*/true);
    }
  }
}

ContentSetting GetSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType content_type,
    jstring origin,
    jstring embedder) {
  GURL url(ConvertJavaStringToUTF8(env, origin));
  std::string embedder_str = ConvertJavaStringToUTF8(env, embedder);
  GURL embedder_url;
  // TODO(raymes): This check to see if '*' is the embedder is a hack that fixes
  // crbug.com/738377. In general querying the settings for patterns is broken
  // and needs to be fixed. See crbug.com/738757.
  if (embedder_str == "*")
    embedder_url = url;
  else
    embedder_url = GURL(embedder_str);
  return permissions::PermissionsClient::Get()
      ->GetPermissionManager(unwrap(jbrowser_context_handle))
      ->GetPermissionStatus(content_type, url, embedder_url)
      .content_setting;
}

void SetSettingForOrigin(JNIEnv* env,
                         const JavaParamRef<jobject>& jbrowser_context_handle,
                         ContentSettingsType content_type,
                         jstring origin,
                         jstring embedder,
                         ContentSetting setting) {
  GURL origin_url(ConvertJavaStringToUTF8(env, origin));
  GURL embedder_url =
      embedder ? GURL(ConvertJavaStringToUTF8(env, embedder)) : GURL();
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);

  // The permission may have been blocked due to being under embargo, so if it
  // was changed away from BLOCK, clear embargo status if it exists.
  if (setting != CONTENT_SETTING_BLOCK) {
    permissions::PermissionsClient::Get()
        ->GetPermissionDecisionAutoBlocker(browser_context)
        ->RemoveEmbargoAndResetCounts(origin_url, content_type);
  }

  if (MaybeResetDSEPermission(browser_context, content_type, origin_url,
                              embedder_url, setting)) {
    return;
  }

  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          browser_context, origin_url, embedder_url, content_type,
          permissions::PermissionSourceUI::SITE_SETTINGS);
  GetHostContentSettingsMap(browser_context)
      ->SetContentSettingDefaultScope(origin_url, embedder_url, content_type,
                                      std::string(), setting);
  content_settings::LogWebSiteSettingsPermissionChange(content_type, setting);
}

permissions::ChooserContextBase* GetChooserContext(
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType type) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  return permissions::PermissionsClient::Get()->GetChooserContext(
      browser_context, type);
}

bool OriginMatcher(const url::Origin& origin, const GURL& other) {
  return origin == url::Origin::Create(other);
}

bool GetBooleanForContentSetting(
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType type) {
  HostContentSettingsMap* content_settings =
      GetHostContentSettingsMap(jbrowser_context_handle);
  switch (content_settings->GetDefaultContentSetting(type, nullptr)) {
    case CONTENT_SETTING_BLOCK:
      return false;
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_ASK:
    default:
      return true;
  }
}

bool IsContentSettingManaged(
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      GetHostContentSettingsMap(jbrowser_context_handle);
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider == HostContentSettingsMap::POLICY_PROVIDER;
}

bool IsContentSettingManagedByCustodian(
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      GetHostContentSettingsMap(jbrowser_context_handle);
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider == HostContentSettingsMap::SUPERVISED_PROVIDER;
}

bool IsContentSettingUserModifiable(
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      GetHostContentSettingsMap(jbrowser_context_handle);
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider >= HostContentSettingsMap::PREF_PROVIDER;
}

}  // anonymous namespace

static void JNI_WebsitePreferenceBridge_GetClipboardOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(
      env, jbrowser_context_handle, ContentSettingsType::CLIPBOARD_READ_WRITE,
      &Java_WebsitePreferenceBridge_insertClipboardInfoIntoList, list, false);
}

static jint JNI_WebsitePreferenceBridge_GetClipboardSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::CLIPBOARD_READ_WRITE, origin,
                             origin);
}

static void JNI_WebsitePreferenceBridge_SetClipboardSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::CLIPBOARD_READ_WRITE, origin, origin,
                      static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetGeolocationOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list,
    jboolean managedOnly) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::GEOLOCATION,
             &Java_WebsitePreferenceBridge_insertGeolocationInfoIntoList, list,
             managedOnly);
}

static jint JNI_WebsitePreferenceBridge_GetGeolocationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::GEOLOCATION, origin,
                             embedder);
}

static void JNI_WebsitePreferenceBridge_SetGeolocationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::GEOLOCATION, origin, embedder,
                      static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetIdleDetectionOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::IDLE_DETECTION,
             &Java_WebsitePreferenceBridge_insertIdleDetectionInfoIntoList,
             list, false);
}

static jint JNI_WebsitePreferenceBridge_GetIdleDetectionSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::IDLE_DETECTION, origin,
                             embedder);
}

static void JNI_WebsitePreferenceBridge_SetIdleDetectionSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::IDLE_DETECTION, origin, embedder,
                      static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetMidiOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::MIDI_SYSEX,
             &Java_WebsitePreferenceBridge_insertMidiInfoIntoList, list, false);
}

static jint JNI_WebsitePreferenceBridge_GetMidiSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::MIDI_SYSEX, origin, embedder);
}

static void JNI_WebsitePreferenceBridge_SetMidiSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::MIDI_SYSEX, origin, embedder,
                      static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetProtectedMediaIdentifierOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(
      env, jbrowser_context_handle,
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
      &Java_WebsitePreferenceBridge_insertProtectedMediaIdentifierInfoIntoList,
      list, false);
}

static jint
JNI_WebsitePreferenceBridge_GetProtectedMediaIdentifierSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER,
                             origin, embedder);
}

static void
JNI_WebsitePreferenceBridge_SetProtectedMediaIdentifierSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER, origin,
                      embedder, static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetNotificationOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::NOTIFICATIONS,
             &Java_WebsitePreferenceBridge_insertNotificationIntoList, list,
             false);
}

static jint JNI_WebsitePreferenceBridge_GetNotificationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::NOTIFICATIONS, origin,
                             origin);
}

static jboolean JNI_WebsitePreferenceBridge_IsNotificationEmbargoedForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin) {
  GURL origin_url(ConvertJavaStringToUTF8(env, origin));
  permissions::PermissionResult status =
      permissions::PermissionsClient::Get()
          ->GetPermissionManager(unwrap(jbrowser_context_handle))
          ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS, origin_url,
                                origin_url);
  return status.content_setting == ContentSetting::CONTENT_SETTING_BLOCK &&
         (status.source ==
              permissions::PermissionStatusSource::MULTIPLE_IGNORES ||
          status.source ==
              permissions::PermissionStatusSource::MULTIPLE_DISMISSALS);
}

static void JNI_WebsitePreferenceBridge_SetNotificationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    jint value) {
  // Note: For Android O+, SetNotificationSettingForOrigin is only called when:
  //  1) the "Clear & Reset" button in Site Settings is pressed,
  //  2) the notification permission is blocked by embargo, so no notification
  //     channel exists yet, and in this state the user changes the setting to
  //     allow or "real" block in SingleWebsitePreferences.
  // Otherwise, we rely on ReportNotificationRevokedForOrigin to explicitly
  // record metrics when we detect changes initiated in Android.
  //
  // Note: Web Notification permission behaves differently from all other
  // permission types. See https://crbug.com/416894.
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  GURL url = GURL(ConvertJavaStringToUTF8(env, origin));
  ContentSetting setting = static_cast<ContentSetting>(value);

  permissions::PermissionsClient::Get()
      ->GetPermissionDecisionAutoBlocker(browser_context)
      ->RemoveEmbargoAndResetCounts(url, ContentSettingsType::NOTIFICATIONS);

  if (MaybeResetDSEPermission(browser_context,
                              ContentSettingsType::NOTIFICATIONS, url, GURL(),
                              setting)) {
    return;
  }

  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          browser_context, url, GURL(), ContentSettingsType::NOTIFICATIONS,
          permissions::PermissionSourceUI::SITE_SETTINGS);

  GetHostContentSettingsMap(browser_context)
      ->SetContentSettingDefaultScope(
          url, GURL(), ContentSettingsType::NOTIFICATIONS,
          content_settings::ResourceIdentifier(), setting);
  content_settings::LogWebSiteSettingsPermissionChange(
      ContentSettingsType::NOTIFICATIONS, setting);
}

// In Android O+, Android is responsible for revoking notification settings--
// We detect this change and explicitly report it back for UMA reporting.
static void JNI_WebsitePreferenceBridge_ReportNotificationRevokedForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    jint new_setting_value) {
  GURL url = GURL(ConvertJavaStringToUTF8(env, origin));

  ContentSetting setting = static_cast<ContentSetting>(new_setting_value);
  DCHECK_NE(setting, CONTENT_SETTING_ALLOW);

  content_settings::LogWebSiteSettingsPermissionChange(
      ContentSettingsType::NOTIFICATIONS, setting);

  permissions::PermissionUmaUtil::PermissionRevoked(
      ContentSettingsType::NOTIFICATIONS,
      permissions::PermissionSourceUI::ANDROID_SETTINGS, url.GetOrigin(),
      unwrap(jbrowser_context_handle));
}

static void JNI_WebsitePreferenceBridge_GetCameraOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list,
    jboolean managedOnly) {
  GetOrigins(env, jbrowser_context_handle,
             ContentSettingsType::MEDIASTREAM_CAMERA,
             &Java_WebsitePreferenceBridge_insertCameraInfoIntoList, list,
             managedOnly);
}

static void JNI_WebsitePreferenceBridge_GetMicrophoneOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list,
    jboolean managedOnly) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::MEDIASTREAM_MIC,
             &Java_WebsitePreferenceBridge_insertMicrophoneInfoIntoList, list,
             managedOnly);
}

static jint JNI_WebsitePreferenceBridge_GetMicrophoneSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::MEDIASTREAM_MIC, origin,
                             embedder);
}

static jint JNI_WebsitePreferenceBridge_GetCameraSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::MEDIASTREAM_CAMERA, origin,
                             embedder);
}

static void JNI_WebsitePreferenceBridge_SetMicrophoneSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    jint value) {
  // Here 'nullptr' indicates that microphone uses wildcard for embedder.
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::MEDIASTREAM_MIC, origin, nullptr,
                      static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_SetCameraSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    jint value) {
  // Here 'nullptr' indicates that camera uses wildcard for embedder.
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::MEDIASTREAM_CAMERA, origin, nullptr,
                      static_cast<ContentSetting>(value));
}

static jboolean JNI_WebsitePreferenceBridge_IsContentSettingsPatternValid(
    JNIEnv* env,
    const JavaParamRef<jstring>& pattern) {
  return ContentSettingsPattern::FromString(
             ConvertJavaStringToUTF8(env, pattern))
      .IsValid();
}

static jboolean JNI_WebsitePreferenceBridge_UrlMatchesContentSettingsPattern(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jstring>& jpattern) {
  ContentSettingsPattern pattern = ContentSettingsPattern::FromString(
      ConvertJavaStringToUTF8(env, jpattern));
  return pattern.Matches(GURL(ConvertJavaStringToUTF8(env, jurl)));
}

static void JNI_WebsitePreferenceBridge_GetChosenObjects(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint content_settings_type,
    const JavaParamRef<jobject>& list) {
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);
  permissions::ChooserContextBase* context =
      GetChooserContext(jbrowser_context_handle, type);
  // The ChooserContextBase can be null if the embedder doesn't support the
  // given ContentSettingsType.
  if (!context)
    return;
  for (const auto& object : context->GetAllGrantedObjects()) {
    // Remove the trailing slash so that origins are matched correctly in
    // SingleWebsitePreferences.mergePermissionInfoForTopLevelOrigin.
    std::string origin = object->requesting_origin.spec();
    DCHECK_EQ('/', origin.back());
    origin.pop_back();
    ScopedJavaLocalRef<jstring> jorigin = ConvertUTF8ToJavaString(env, origin);

    std::string embedder = object->embedding_origin.spec();
    DCHECK_EQ('/', embedder.back());
    embedder.pop_back();
    ScopedJavaLocalRef<jstring> jembedder;
    if (embedder != origin)
      jembedder = ConvertUTF8ToJavaString(env, embedder);

    ScopedJavaLocalRef<jstring> jname = ConvertUTF16ToJavaString(
        env, context->GetObjectDisplayName(object->value));

    std::string serialized;
    bool written = base::JSONWriter::Write(object->value, &serialized);
    DCHECK(written);
    ScopedJavaLocalRef<jstring> jserialized =
        ConvertUTF8ToJavaString(env, serialized);

    jboolean jis_managed =
        object->source == content_settings::SETTING_SOURCE_POLICY;

    Java_WebsitePreferenceBridge_insertChosenObjectInfoIntoList(
        env, list, content_settings_type, jorigin, jembedder, jname,
        jserialized, jis_managed);
  }
}

static void JNI_WebsitePreferenceBridge_RevokeObjectPermission(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint content_settings_type,
    const JavaParamRef<jstring>& jorigin,
    const JavaParamRef<jstring>& jembedder,
    const JavaParamRef<jstring>& jobject) {
  GURL origin(ConvertJavaStringToUTF8(env, jorigin));
  DCHECK(origin.is_valid());
  // If embedder == origin above then a null embedder was sent to Java instead
  // of a duplicated string.
  GURL embedder(
      ConvertJavaStringToUTF8(env, jembedder.is_null() ? jorigin : jembedder));
  DCHECK(embedder.is_valid());
  std::unique_ptr<base::DictionaryValue> object = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(ConvertJavaStringToUTF8(env, jobject)));
  DCHECK(object);
  permissions::ChooserContextBase* context = GetChooserContext(
      jbrowser_context_handle,
      static_cast<ContentSettingsType>(content_settings_type));
  context->RevokeObjectPermission(url::Origin::Create(origin),
                                  url::Origin::Create(embedder), *object);
}

namespace {

void OnCookiesReceived(network::mojom::CookieManager* cookie_manager,
                       const GURL& domain,
                       const std::vector<net::CanonicalCookie>& cookies) {
  for (const auto& cookie : cookies) {
    if (cookie.IsDomainMatch(domain.host())) {
      cookie_manager->DeleteCanonicalCookie(cookie, base::DoNothing());
    }
  }
}

void OnStorageInfoReady(const ScopedJavaGlobalRef<jobject>& java_callback,
                        const storage::UsageInfoEntries& entries) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> list =
      Java_WebsitePreferenceBridge_createStorageInfoList(env);

  storage::UsageInfoEntries::const_iterator i;
  for (i = entries.begin(); i != entries.end(); ++i) {
    if (i->usage <= 0)
      continue;
    ScopedJavaLocalRef<jstring> host = ConvertUTF8ToJavaString(env, i->host);

    Java_WebsitePreferenceBridge_insertStorageInfoIntoList(
        env, list, host, static_cast<jint>(i->type), i->usage);
  }

  base::android::RunObjectCallbackAndroid(java_callback, list);
}

void OnLocalStorageCleared(const ScopedJavaGlobalRef<jobject>& java_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Java_StorageInfoClearedCallback_onStorageInfoCleared(
      base::android::AttachCurrentThread(), java_callback);
}

void OnStorageInfoCleared(const ScopedJavaGlobalRef<jobject>& java_callback,
                          blink::mojom::QuotaStatusCode code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Java_StorageInfoClearedCallback_onStorageInfoCleared(
      base::android::AttachCurrentThread(), java_callback);
}

void OnLocalStorageModelInfoLoaded(
    BrowserContext* browser_context,
    bool fetch_important,
    const ScopedJavaGlobalRef<jobject>& java_callback,
    const std::list<content::StorageUsageInfo>& local_storage_info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> map =
      Java_WebsitePreferenceBridge_createLocalStorageInfoMap(env);

  std::vector<std::pair<url::Origin, bool>> important_notations(
      local_storage_info.size());
  std::transform(local_storage_info.begin(), local_storage_info.end(),
                 important_notations.begin(),
                 [](const content::StorageUsageInfo& info) {
                   return std::make_pair(info.origin, false);
                 });
  if (fetch_important) {
    permissions::PermissionsClient::Get()->AreSitesImportant(
        browser_context, &important_notations);
  }

  int i = 0;
  for (const content::StorageUsageInfo& info : local_storage_info) {
    ScopedJavaLocalRef<jstring> java_origin =
        ConvertUTF8ToJavaString(env, info.origin.Serialize());
    Java_WebsitePreferenceBridge_insertLocalStorageInfoIntoMap(
        env, map, java_origin, info.total_size_bytes,
        important_notations[i++].second);
  }

  base::android::RunObjectCallbackAndroid(java_callback, map);
}

}  // anonymous namespace

// TODO(jknotten): These methods should not be static. Instead we should
// expose a class to Java so that the fetch requests can be cancelled,
// and manage the lifetimes of the callback (and indirectly the helper
// by having a reference to it).

// The helper methods (StartFetching, DeleteLocalStorageFile, DeleteDatabase)
// are asynchronous. A "use after free" error is not possible because the
// helpers keep a reference to themselves for the duration of their tasks,
// which includes callback invocation.

static void JNI_WebsitePreferenceBridge_FetchLocalStorageInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& java_callback,
    jboolean fetch_important) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  auto local_storage_helper =
      base::MakeRefCounted<browsing_data::LocalStorageHelper>(browser_context);
  local_storage_helper->StartFetching(base::BindOnce(
      &OnLocalStorageModelInfoLoaded, browser_context, fetch_important,
      ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_WebsitePreferenceBridge_FetchStorageInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& java_callback) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);

  auto storage_info_fetcher =
      base::MakeRefCounted<browser_ui::StorageInfoFetcher>(browser_context);
  storage_info_fetcher->FetchStorageInfo(base::BindOnce(
      &OnStorageInfoReady, ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_WebsitePreferenceBridge_ClearLocalStorageData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin,
    const JavaParamRef<jobject>& java_callback) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  auto local_storage_helper =
      base::MakeRefCounted<browsing_data::LocalStorageHelper>(browser_context);
  auto origin =
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(env, jorigin)));
  local_storage_helper->DeleteOrigin(
      origin, base::BindOnce(&OnLocalStorageCleared,
                             ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_WebsitePreferenceBridge_ClearStorageData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jhost,
    jint type,
    const JavaParamRef<jobject>& java_callback) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  std::string host = ConvertJavaStringToUTF8(env, jhost);

  auto storage_info_fetcher =
      base::MakeRefCounted<browser_ui::StorageInfoFetcher>(browser_context);
  storage_info_fetcher->ClearStorage(
      host, static_cast<blink::mojom::StorageType>(type),
      base::BindOnce(&OnStorageInfoCleared,
                     ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_WebsitePreferenceBridge_ClearCookieData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  GURL url(ConvertJavaStringToUTF8(env, jorigin));

  auto* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(browser_context);
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  cookie_manager->GetAllCookies(
      base::BindOnce(&OnCookiesReceived, cookie_manager, url));
}

static void JNI_WebsitePreferenceBridge_ClearBannerData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin) {
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->SetWebsiteSettingDefaultScope(
          GURL(ConvertJavaStringToUTF8(env, jorigin)), GURL(),
          ContentSettingsType::APP_BANNER, std::string(), nullptr);
}

static void JNI_WebsitePreferenceBridge_ClearMediaLicenses(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  url::Origin origin =
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(env, jorigin)));
  cdm::MediaDrmStorageImpl::ClearMatchingLicenses(
      user_prefs::UserPrefs::Get(browser_context), base::Time(),
      base::Time::Max(), base::BindRepeating(&OriginMatcher, origin),
      base::DoNothing());
}

static jboolean JNI_WebsitePreferenceBridge_IsPermissionControlledByDSE(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jstring>& jorigin) {
  return permissions::PermissionsClient::Get()->IsPermissionControlledByDse(
      unwrap(jbrowser_context_handle),
      static_cast<ContentSettingsType>(content_settings_type),
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(env, jorigin))));
}

static jboolean JNI_WebsitePreferenceBridge_GetAdBlockingActivated(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin) {
  GURL url(ConvertJavaStringToUTF8(env, jorigin));
  return permissions::PermissionsClient::Get()->IsSubresourceFilterActivated(
      unwrap(jbrowser_context_handle), url);
}

static void JNI_WebsitePreferenceBridge_GetArOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::AR,
             &Java_WebsitePreferenceBridge_insertArInfoIntoList, list, false);
}

static jint JNI_WebsitePreferenceBridge_GetArSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::AR, origin, embedder);
}

static void JNI_WebsitePreferenceBridge_SetArSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle, ContentSettingsType::AR,
                      origin, embedder, static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetNfcOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::NFC,
             &Java_WebsitePreferenceBridge_insertNfcInfoIntoList, list, false);
}

static jint JNI_WebsitePreferenceBridge_GetNfcSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::NFC, origin, embedder);
}

static void JNI_WebsitePreferenceBridge_SetNfcSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle, ContentSettingsType::NFC,
                      origin, embedder, static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetSensorsOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::SENSORS,
             &Java_WebsitePreferenceBridge_insertSensorsInfoIntoList, list,
             false);
}

static jint JNI_WebsitePreferenceBridge_GetSensorsSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::SENSORS, origin, embedder);
}

static void JNI_WebsitePreferenceBridge_SetSensorsSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle,
                      ContentSettingsType::SENSORS, origin, embedder,
                      static_cast<ContentSetting>(value));
}

static void JNI_WebsitePreferenceBridge_GetVrOrigins(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& list) {
  GetOrigins(env, jbrowser_context_handle, ContentSettingsType::VR,
             &Java_WebsitePreferenceBridge_insertVrInfoIntoList, list, false);
}

static jint JNI_WebsitePreferenceBridge_GetVrSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  return GetSettingForOrigin(env, jbrowser_context_handle,
                             ContentSettingsType::VR, origin, embedder);
}

static void JNI_WebsitePreferenceBridge_SetVrSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  SetSettingForOrigin(env, jbrowser_context_handle, ContentSettingsType::VR,
                      origin, embedder, static_cast<ContentSetting>(value));
}

// On Android O+ notification channels are not stored in the Chrome profile and
// so are persisted across tests. This function resets them.
static void JNI_WebsitePreferenceBridge_ResetNotificationsSettingsForTest(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle) {
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->ClearSettingsForOneType(ContentSettingsType::NOTIFICATIONS);
}

static jboolean JNI_WebsitePreferenceBridge_IsContentSettingManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type) {
  return IsContentSettingManaged(
      jbrowser_context_handle,
      static_cast<ContentSettingsType>(content_settings_type));
}

static jboolean JNI_WebsitePreferenceBridge_IsCookieDeletionDisabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin) {
  std::string origin = ConvertJavaStringToUTF8(env, jorigin);
  return permissions::PermissionsClient::Get()->IsCookieDeletionDisabled(
      unwrap(jbrowser_context_handle), GURL(origin));
}

static jboolean JNI_WebsitePreferenceBridge_IsContentSettingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type) {
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);

  return GetBooleanForContentSetting(jbrowser_context_handle, type);
}

static void JNI_WebsitePreferenceBridge_SetContentSettingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    jboolean allow) {
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);

  if (type == ContentSettingsType::SOUND) {
    if (allow) {
      base::RecordAction(base::UserMetricsAction(
          "SoundContentSetting.UnmuteBy.DefaultSwitch"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("SoundContentSetting.MuteBy.DefaultSwitch"));
    }
  }

  ContentSetting value = CONTENT_SETTING_BLOCK;
  if (allow) {
    switch (type) {
      case ContentSettingsType::AR:
      case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      case ContentSettingsType::BLUETOOTH_GUARD:
      case ContentSettingsType::BLUETOOTH_SCANNING:
      case ContentSettingsType::CLIPBOARD_READ_WRITE:
      case ContentSettingsType::GEOLOCATION:
      case ContentSettingsType::IDLE_DETECTION:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::NFC:
      case ContentSettingsType::NOTIFICATIONS:
      case ContentSettingsType::USB_GUARD:
      case ContentSettingsType::VR:
        value = CONTENT_SETTING_ASK;
        break;
      case ContentSettingsType::ADS:
      case ContentSettingsType::BACKGROUND_SYNC:
      case ContentSettingsType::COOKIES:
      case ContentSettingsType::JAVASCRIPT:
      case ContentSettingsType::POPUPS:
      case ContentSettingsType::SENSORS:
      case ContentSettingsType::SOUND:
        value = CONTENT_SETTING_ALLOW;
        break;
      default:
        NOTREACHED() << static_cast<int>(type);  // Not supported on Android.
    }
  }

  GetHostContentSettingsMap(jbrowser_context_handle)
      ->SetDefaultContentSetting(type, value);
}

static void JNI_WebsitePreferenceBridge_SetContentSettingForPattern(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jstring>& primary_pattern,
    const JavaParamRef<jstring>& secondary_pattern,
    int setting) {
  std::string primary_pattern_string =
      ConvertJavaStringToUTF8(env, primary_pattern);
  std::string secondary_pattern_string =
      ConvertJavaStringToUTF8(env, secondary_pattern);
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromString(primary_pattern_string),
          secondary_pattern_string.empty()
              ? ContentSettingsPattern::Wildcard()
              : ContentSettingsPattern::FromString(secondary_pattern_string),
          static_cast<ContentSettingsType>(content_settings_type),
          std::string(), static_cast<ContentSetting>(setting));
}

static void JNI_WebsitePreferenceBridge_GetContentSettingsExceptions(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jobject>& list) {
  ContentSettingsForOneType entries;
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->GetSettingsForOneType(
          static_cast<ContentSettingsType>(content_settings_type), "",
          &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    Java_WebsitePreferenceBridge_addContentSettingExceptionToList(
        env, list, content_settings_type,
        ConvertUTF8ToJavaString(env, entries[i].primary_pattern.ToString()),
        ConvertUTF8ToJavaString(env, entries[i].secondary_pattern.ToString()),
        entries[i].GetContentSetting(),
        ConvertUTF8ToJavaString(env, entries[i].source));
  }
}

static jint JNI_WebsitePreferenceBridge_GetContentSetting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type) {
  return GetHostContentSettingsMap(jbrowser_context_handle)
      ->GetDefaultContentSetting(
          static_cast<ContentSettingsType>(content_settings_type), nullptr);
}

static void JNI_WebsitePreferenceBridge_SetContentSetting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    int setting) {
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->SetDefaultContentSetting(
          static_cast<ContentSettingsType>(content_settings_type),
          static_cast<ContentSetting>(setting));
}

static jboolean JNI_WebsitePreferenceBridge_IsContentSettingUserModifiable(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type) {
  return IsContentSettingUserModifiable(
      jbrowser_context_handle,
      static_cast<ContentSettingsType>(content_settings_type));
}

static jboolean JNI_WebsitePreferenceBridge_IsContentSettingManagedByCustodian(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type) {
  return IsContentSettingManagedByCustodian(
      jbrowser_context_handle,
      static_cast<ContentSettingsType>(content_settings_type));
}

static jboolean JNI_WebsitePreferenceBridge_GetLocationAllowedByPolicy(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle) {
  if (!IsContentSettingManaged(jbrowser_context_handle,
                               ContentSettingsType::GEOLOCATION))
    return false;
  return GetHostContentSettingsMap(jbrowser_context_handle)
             ->GetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                        nullptr) == CONTENT_SETTING_ALLOW;
}

