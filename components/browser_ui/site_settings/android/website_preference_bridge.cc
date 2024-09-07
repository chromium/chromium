// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <set>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/browser_ui/site_settings/android/storage_info_fetcher.h"
#include "components/browser_ui/site_settings/android/website_preference_bridge_util.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/cdm/browser/media_drm_storage_impl.h"
#include "components/content_settings/browser/ui/cookie_controls_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "url/url_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/site_settings/android/site_settings_jni_headers/WebsitePreferenceBridge_jni.h"

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
using content_settings::CookieControlsUtil;

namespace {

const char kHttpPortSuffix[] = ":80";
const char kHttpsPortSuffix[] = ":443";

BrowserContext* unwrap(const JavaParamRef<jobject>& jbrowser_context_handle) {
  return content::BrowserContextFromJavaHandle(jbrowser_context_handle);
}

HostContentSettingsMap* GetHostContentSettingsMap(
    BrowserContext* browser_context) {
  return permissions::PermissionsClient::Get()->GetSettingsMap(browser_context);
}

HostContentSettingsMap* GetHostContentSettingsMap(
    const JavaParamRef<jobject>& jbrowser_context_handle) {
  return GetHostContentSettingsMap(unwrap(jbrowser_context_handle));
}

permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
    BrowserContext* browser_context) {
  return permissions::PermissionsClient::Get()
      ->GetPermissionDecisionAutoBlocker(browser_context);
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
    JniIntWrapper,
    const base::android::JavaRef<jobject>&,
    const base::android::JavaRef<jstring>&,
    const base::android::JavaRef<jstring>&,
    jboolean,
    JniIntWrapper);

void GetOrigins(JNIEnv* env,
                const JavaParamRef<jobject>& jbrowser_context_handle,
                ContentSettingsType content_type,
                InfoListInsertionFunction insertionFunc,
                const JavaRef<jobject>& list,
                jboolean managedOnly) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  HostContentSettingsMap* content_settings_map =
      GetHostContentSettingsMap(browser_context);

  ContentSettingsForOneType all_settings =
      content_settings_map->GetSettingsForOneType(content_type);
  ContentSettingsForOneType embargo_settings =
      content_settings_map->GetSettingsForOneType(
          ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA);
  ContentSetting default_content_setting =
      content_settings_map->GetDefaultContentSetting(content_type, nullptr);

  // Use a vector since the overall number of origins should be small.
  std::vector<std::string> seen_origins;

  // Now add all origins that have a non-default setting to the list.
  for (const auto& settings_it : all_settings) {
    if (settings_it.GetContentSetting() == default_content_setting)
      continue;
    if (managedOnly &&
        settings_it.source != content_settings::ProviderType::kPolicyProvider) {
      continue;
    }
    const std::string origin = settings_it.primary_pattern.ToString();
    const std::string embedder = settings_it.secondary_pattern.ToString();

    ScopedJavaLocalRef<jstring> jembedder;
    if (embedder != origin)
      jembedder = ConvertUTF8ToJavaString(env, embedder);

    seen_origins.push_back(origin);
    insertionFunc(env, static_cast<int>(content_type), list,
                  ConvertOriginToJavaString(env, origin), jembedder,
                  /*is_embargoed=*/false,
                  static_cast<int>(settings_it.metadata.session_model()));
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

    if (auto_blocker->IsEmbargoed(GURL(origin), content_type)) {
      seen_origins.push_back(origin);
      insertionFunc(env, static_cast<int>(content_type), list,
                    ConvertOriginToJavaString(env, origin), jembedder,
                    /*is_embargoed=*/true, /*is_one_time=*/false);
    }
  }
}

ContentSetting GetPermissionSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType content_type,
    jstring origin,
    jstring embedder) {
  GURL requesting_origin(ConvertJavaStringToUTF8(env, origin));
  std::string embedder_str = ConvertJavaStringToUTF8(env, embedder);
  GURL embedding_origin;
  // TODO(raymes): This check to see if '*' is the embedder is a hack that fixes
  // crbug.com/738377. In general querying the settings for patterns is broken
  // and needs to be fixed. See crbug.com/738757.
  if (embedder_str == "*")
    embedding_origin = requesting_origin;
  else
    embedding_origin = GURL(embedder_str);

  // If `content_type` is permission, then we need to apply a set of
  // verifications before reading its value in `HostContentSettingsMap`.
  if (permissions::PermissionUtil::IsPermission(content_type)) {
    BrowserContext* browser_context = unwrap(jbrowser_context_handle);
    content::PermissionController* permission_controller =
        browser_context->GetPermissionController();
    content::PermissionResult result =
        permission_controller->GetPermissionResultForOriginWithoutContext(
            permissions::PermissionUtil::ContentSettingTypeToPermissionType(
                content_type),
            url::Origin::Create(requesting_origin),
            url::Origin::Create(embedding_origin));
    return permissions::PermissionUtil::PermissionStatusToContentSetting(
        result.status);
  } else {
    // If `content_type` is not permission, then we can directly read its value
    // from `HostContentSettingsMap`.
    HostContentSettingsMap* host_content_settings_map =
        GetHostContentSettingsMap(jbrowser_context_handle);
    return host_content_settings_map->GetContentSetting(
        requesting_origin, embedding_origin, content_type);
  }
}

void SetPermissionSettingForOrigin(
    JNIEnv* env,
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
    GetPermissionDecisionAutoBlocker(browser_context)
        ->RemoveEmbargoAndResetCounts(origin_url, content_type);
  }

  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          browser_context, origin_url, embedder_url, content_type,
          permissions::PermissionSourceUI::SITE_SETTINGS);
  GetHostContentSettingsMap(browser_context)
      ->SetContentSettingDefaultScope(origin_url, embedder_url, content_type,
                                      setting);
}

permissions::ObjectPermissionContextBase* GetChooserContext(
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
  HostContentSettingsMap* content_settings =
      GetHostContentSettingsMap(jbrowser_context_handle);
  content_settings::ProviderType provider;
  content_settings->GetDefaultContentSetting(content_settings_type, &provider);
  return provider == content_settings::ProviderType::kPolicyProvider;
}

bool IsContentSettingManagedByCustodian(
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType content_settings_type) {
  HostContentSettingsMap* content_settings =
      GetHostContentSettingsMap(jbrowser_context_handle);
  content_settings::ProviderType provider;
  content_settings->GetDefaultContentSetting(content_settings_type, &provider);
  return provider == content_settings::ProviderType::kSupervisedProvider;
}

bool IsContentSettingUserModifiable(
    const JavaParamRef<jobject>& jbrowser_context_handle,
    ContentSettingsType content_settings_type) {
  HostContentSettingsMap* content_settings =
      GetHostContentSettingsMap(jbrowser_context_handle);
  content_settings::ProviderType provider;
  content_settings->GetDefaultContentSetting(content_settings_type, &provider);
  return provider >= content_settings::ProviderType::kPrefProvider;
}

}  // anonymous namespace

static jboolean JNI_WebsitePreferenceBridge_IsNotificationEmbargoedForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& origin) {
  GURL origin_url(ConvertJavaStringToUTF8(env, origin));
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      GetPermissionDecisionAutoBlocker(browser_context);

  return auto_blocker->IsEmbargoed(origin_url,
                                   ContentSettingsType::NOTIFICATIONS);
}

static void SetNotificationSettingForOrigin(
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

  GetPermissionDecisionAutoBlocker(browser_context)
      ->RemoveEmbargoAndResetCounts(url, ContentSettingsType::NOTIFICATIONS);

  permissions::PermissionUmaUtil::ScopedRevocationReporter
      scoped_revocation_reporter(
          browser_context, url, GURL(), ContentSettingsType::NOTIFICATIONS,
          permissions::PermissionSourceUI::SITE_SETTINGS);

  GetHostContentSettingsMap(browser_context)
      ->SetContentSettingDefaultScope(
          url, GURL(), ContentSettingsType::NOTIFICATIONS, setting);
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

  permissions::PermissionUmaUtil::PermissionRevoked(
      ContentSettingsType::NOTIFICATIONS,
      permissions::PermissionSourceUI::ANDROID_SETTINGS,
      url.DeprecatedGetOriginAsURL(), unwrap(jbrowser_context_handle));
}

static jint JNI_WebsitePreferenceBridge_GetPermissionSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint content_settings_type,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder) {
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);
  return GetPermissionSettingForOrigin(env, jbrowser_context_handle, type,
                                       origin, embedder);
}

static void JNI_WebsitePreferenceBridge_SetPermissionSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint content_settings_type,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value) {
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);

  switch (type) {
    case ContentSettingsType::NOTIFICATIONS:
      return SetNotificationSettingForOrigin(env, jbrowser_context_handle,
                                             origin, value);
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return SetPermissionSettingForOrigin(env, jbrowser_context_handle, type,
                                           origin, nullptr,
                                           static_cast<ContentSetting>(value));
    default:
      SetPermissionSettingForOrigin(env, jbrowser_context_handle, type, origin,
                                    embedder,
                                    static_cast<ContentSetting>(value));
  }
}

static void JNI_WebsitePreferenceBridge_SetEphemeralGrantForTesting(  // IN-TEST
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint content_settings_type,
    const JavaParamRef<jobject>& jprimary_url,
    const JavaParamRef<jobject>& jsecondary_url) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(
      content_settings::mojom::SessionModel::ONE_TIME);
  GetHostContentSettingsMap(browser_context)
      ->SetContentSettingDefaultScope(
          url::GURLAndroid::ToNativeGURL(env, jprimary_url),
          url::GURLAndroid::ToNativeGURL(env, jsecondary_url),
          static_cast<ContentSettingsType>(content_settings_type),
          CONTENT_SETTING_ALLOW, constraints);
}

static void JNI_WebsitePreferenceBridge_GetOriginsForPermission(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint content_settings_type,
    const JavaParamRef<jobject>& list,
    jboolean managedOnly) {
  GetOrigins(env, jbrowser_context_handle,
             static_cast<ContentSettingsType>(content_settings_type),
             &Java_WebsitePreferenceBridge_insertPermissionInfoIntoList, list,
             managedOnly);
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
  permissions::ObjectPermissionContextBase* context =
      GetChooserContext(jbrowser_context_handle, type);
  // The ObjectPermissionContextBase can be null if the embedder doesn't support
  // the given ContentSettingsType.
  if (!context)
    return;
  for (const auto& object : context->GetAllGrantedObjects()) {
    // Remove the trailing slash so that origins are matched correctly in
    // SingleWebsitePreferences.mergePermissionInfoForTopLevelOrigin.
    std::string origin = object->origin.spec();
    DCHECK_EQ('/', origin.back());
    origin.pop_back();
    ScopedJavaLocalRef<jstring> jorigin = ConvertUTF8ToJavaString(env, origin);

    ScopedJavaLocalRef<jstring> jname = ConvertUTF16ToJavaString(
        env, context->GetObjectDisplayName(object->value));

    std::string serialized;
    bool written = base::JSONWriter::Write(object->value, &serialized);
    DCHECK(written);
    ScopedJavaLocalRef<jstring> jserialized =
        ConvertUTF8ToJavaString(env, serialized);

    jboolean jis_managed =
        object->source == content_settings::SettingSource::kPolicy;

    Java_WebsitePreferenceBridge_insertChosenObjectInfoIntoList(
        env, list, content_settings_type, jorigin, jname, jserialized,
        jis_managed);
  }
}

static void JNI_WebsitePreferenceBridge_RevokeObjectPermission(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    jint content_settings_type,
    const JavaParamRef<jstring>& jorigin,
    const JavaParamRef<jstring>& jobject) {
  GURL origin(ConvertJavaStringToUTF8(env, jorigin));
  DCHECK(origin.is_valid());
  std::optional<base::Value> object =
      base::JSONReader::Read(ConvertJavaStringToUTF8(env, jobject));
  DCHECK(object && object->is_dict());
  permissions::ObjectPermissionContextBase* context = GetChooserContext(
      jbrowser_context_handle,
      static_cast<ContentSettingsType>(content_settings_type));
  context->RevokeObjectPermission(url::Origin::Create(origin),
                                  object->GetDict());
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

void OnCookiesInfoReady(const ScopedJavaGlobalRef<jobject>& java_callback,
                        const net::CookieList& entries) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> map =
      Java_WebsitePreferenceBridge_createCookiesInfoMap(env);

  for (const net::CanonicalCookie& cookie : entries) {
    std::string origin = net::cookie_util::CookieOriginToURL(
                             cookie.Domain(), cookie.SecureAttribute())
                             .spec();
    ScopedJavaLocalRef<jstring> java_origin =
        ConvertUTF8ToJavaString(env, origin);
    Java_WebsitePreferenceBridge_insertCookieIntoMap(env, map, java_origin);
  }

  base::android::RunObjectCallbackAndroid(java_callback, map);
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
  base::ranges::transform(local_storage_info, important_notations.begin(),
                          [](const content::StorageUsageInfo& info) {
                            return std::make_pair(info.storage_key.origin(),
                                                  false);
                          });
  if (fetch_important) {
    permissions::PermissionsClient::Get()->AreSitesImportant(
        browser_context, &important_notations);
  }

  int i = 0;
  for (const content::StorageUsageInfo& info : local_storage_info) {
    ScopedJavaLocalRef<jstring> java_origin =
        ConvertUTF8ToJavaString(env, info.storage_key.origin().Serialize());
    Java_WebsitePreferenceBridge_insertLocalStorageInfoIntoMap(
        env, map, java_origin, info.total_size_bytes,
        important_notations[i++].second);
  }

  base::android::RunObjectCallbackAndroid(java_callback, map);
}

void OnSharedDictionaryInfoLoaded(
    BrowserContext* browser_context,
    const ScopedJavaGlobalRef<jobject>& java_callback,
    const std::vector<net::SharedDictionaryUsageInfo>& shared_dictionary_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> list =
      Java_WebsitePreferenceBridge_createSharedDictionaryInfoList(env);
  for (const auto& info : shared_dictionary_info) {
    ScopedJavaLocalRef<jstring> java_origin = ConvertUTF8ToJavaString(
        env, info.isolation_key.frame_origin().Serialize());
    ScopedJavaLocalRef<jstring> java_top_frame_site = ConvertUTF8ToJavaString(
        env, info.isolation_key.top_frame_site().Serialize());
    Java_WebsitePreferenceBridge_insertSharedDictionaryInfoIntoList(
        env, list, java_origin, java_top_frame_site, info.total_size_bytes);
  }
  base::android::RunObjectCallbackAndroid(java_callback, list);
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

static void JNI_WebsitePreferenceBridge_FetchCookiesInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& java_callback) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  auto cookie_helper = base::MakeRefCounted<browsing_data::CookieHelper>(
      browser_context->GetDefaultStoragePartition(), base::NullCallback());
  cookie_helper->StartFetching(base::BindOnce(
      &OnCookiesInfoReady, ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_WebsitePreferenceBridge_FetchLocalStorageInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& java_callback,
    jboolean fetch_important) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  auto local_storage_helper =
      base::MakeRefCounted<browsing_data::LocalStorageHelper>(
          browser_context->GetDefaultStoragePartition());
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

static void JNI_WebsitePreferenceBridge_FetchSharedDictionaryInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jobject>& java_callback) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetSharedDictionaryUsageInfo(
          base::BindOnce(&OnSharedDictionaryInfoLoaded, browser_context,
                         ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_WebsitePreferenceBridge_ClearLocalStorageData(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin,
    const JavaParamRef<jobject>& java_callback) {
  ClearLocalStorageHelper::ClearLocalStorage(
      unwrap(jbrowser_context_handle),
      url::Origin::Create(GURL(ConvertJavaStringToUTF8(env, jorigin))),
      base::BindOnce(&OnLocalStorageCleared,
                     ScopedJavaGlobalRef<jobject>(java_callback)));
}

static void JNI_WebsitePreferenceBridge_ClearSharedDictionary(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin,
    const JavaParamRef<jstring>& jtop_level_site,
    const JavaParamRef<jobject>& java_callback) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ClearSharedDictionaryCacheForIsolationKey(
          net::SharedDictionaryIsolationKey(
              url::Origin::Create(GURL(ConvertJavaStringToUTF8(env, jorigin))),
              net::SchemefulSite(
                  GURL(ConvertJavaStringToUTF8(env, jtop_level_site)))),
          base::BindOnce(
              [](const ScopedJavaGlobalRef<jobject>& java_callback) {
                DCHECK_CURRENTLY_ON(BrowserThread::UI);
                Java_StorageInfoClearedCallback_onStorageInfoCleared(
                    base::android::AttachCurrentThread(), java_callback);
              },
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

  auto* storage_partition = browser_context->GetDefaultStoragePartition();
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
          ContentSettingsType::APP_BANNER, base::Value());
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

static jboolean JNI_WebsitePreferenceBridge_IsDSEOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    const JavaParamRef<jstring>& jorigin) {
  return permissions::PermissionsClient::Get()->IsDseOrigin(
      unwrap(jbrowser_context_handle),
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

// On Android O+ notification channels are not stored in the Chrome profile
// and so are persisted across tests. This function resets them.
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
      case ContentSettingsType::HAND_TRACKING:
      case ContentSettingsType::IDLE_DETECTION:
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::MEDIASTREAM_MIC:
      case ContentSettingsType::NFC:
      case ContentSettingsType::NOTIFICATIONS:
      case ContentSettingsType::STORAGE_ACCESS:
      case ContentSettingsType::USB_GUARD:
      case ContentSettingsType::VR:
        value = CONTENT_SETTING_ASK;
        break;
      case ContentSettingsType::ADS:
      case ContentSettingsType::ANTI_ABUSE:
      case ContentSettingsType::AUTO_DARK_WEB_CONTENT:
      case ContentSettingsType::BACKGROUND_SYNC:
      case ContentSettingsType::COOKIES:
      case ContentSettingsType::FEDERATED_IDENTITY_API:
      case ContentSettingsType::JAVASCRIPT:
      case ContentSettingsType::POPUPS:
      case ContentSettingsType::REQUEST_DESKTOP_SITE:
      case ContentSettingsType::SENSORS:
      case ContentSettingsType::SOUND:
        value = CONTENT_SETTING_ALLOW;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << static_cast<int>(type);  // Not supported on Android.
    }
  }

  GetHostContentSettingsMap(jbrowser_context_handle)
      ->SetDefaultContentSetting(type, value);
}

static void JNI_WebsitePreferenceBridge_SetContentSettingDefaultScope(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jobject>& jprimary_url,
    const JavaParamRef<jobject>& jsecondary_url,
    int setting) {
  GURL primary_url = url::GURLAndroid::ToNativeGURL(env, jprimary_url);
  if (setting != CONTENT_SETTING_BLOCK) {
    GetPermissionDecisionAutoBlocker(unwrap(jbrowser_context_handle))
        ->RemoveEmbargoAndResetCounts(
            primary_url,
            static_cast<ContentSettingsType>(content_settings_type));
  }
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->SetContentSettingDefaultScope(
          primary_url, url::GURLAndroid::ToNativeGURL(env, jsecondary_url),
          static_cast<ContentSettingsType>(content_settings_type),
          static_cast<ContentSetting>(setting));
}

static void JNI_WebsitePreferenceBridge_SetContentSettingCustomScope(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jstring>& primary_pattern,
    const JavaParamRef<jstring>& secondary_pattern,
    int setting) {
  std::string primary_pattern_string =
      ConvertJavaStringToUTF8(env, primary_pattern);
  GURL primary_url(primary_pattern_string);
  if (setting != CONTENT_SETTING_BLOCK && primary_url.is_valid()) {
    GetPermissionDecisionAutoBlocker(unwrap(jbrowser_context_handle))
        ->RemoveEmbargoAndResetCounts(
            primary_url,
            static_cast<ContentSettingsType>(content_settings_type));
  }

  std::string secondary_pattern_string =
      ConvertJavaStringToUTF8(env, secondary_pattern);
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromString(primary_pattern_string),
          secondary_pattern_string.empty()
              ? ContentSettingsPattern::Wildcard()
              : ContentSettingsPattern::FromString(secondary_pattern_string),
          static_cast<ContentSettingsType>(content_settings_type),
          static_cast<ContentSetting>(setting));
}

static int JNI_WebsitePreferenceBridge_GetContentSetting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jobject>& jprimary_url,
    const JavaParamRef<jobject>& jsecondary_url) {
  return GetHostContentSettingsMap(jbrowser_context_handle)
      ->GetContentSetting(
          url::GURLAndroid::ToNativeGURL(env, jprimary_url),
          url::GURLAndroid::ToNativeGURL(env, jsecondary_url),
          static_cast<ContentSettingsType>(content_settings_type));
}

static jboolean JNI_WebsitePreferenceBridge_IsContentSettingGlobal(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jobject>& jprimary_url,
    const JavaParamRef<jobject>& jsecondary_url) {
  content_settings::SettingInfo setting_info;
  GetHostContentSettingsMap(jbrowser_context_handle)
      ->GetContentSetting(
          url::GURLAndroid::ToNativeGURL(env, jprimary_url),
          url::GURLAndroid::ToNativeGURL(env, jsecondary_url),
          static_cast<ContentSettingsType>(content_settings_type),
          &setting_info);
  return setting_info.primary_pattern == ContentSettingsPattern::Wildcard() &&
         setting_info.secondary_pattern == ContentSettingsPattern::Wildcard();
}

static void JNI_WebsitePreferenceBridge_GetContentSettingsExceptions(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type,
    const JavaParamRef<jobject>& list) {
  BrowserContext* browser_context = unwrap(jbrowser_context_handle);
  std::vector<std::string> seen_origins;
  for (const ContentSettingPatternSource& entry :
       GetHostContentSettingsMap(browser_context)
           ->GetSettingsForOneType(
               static_cast<ContentSettingsType>(content_settings_type))) {
    std::string origin = entry.primary_pattern.ToString();
    seen_origins.push_back(origin);
    auto hasExpiration = !entry.metadata.expiration().is_null();
    auto expirationInDays = hasExpiration
                                ? CookieControlsUtil::GetDaysToExpiration(
                                      entry.metadata.expiration())
                                : -1;
    Java_WebsitePreferenceBridge_addContentSettingExceptionToList(
        env, list, content_settings_type, ConvertUTF8ToJavaString(env, origin),
        ConvertUTF8ToJavaString(env, entry.secondary_pattern.ToString()),
        entry.GetContentSetting(), static_cast<int>(entry.source),
        hasExpiration, expirationInDays,
        /*is_embargoed=*/false);
  }

  // Add any origins which have been automatically blocked for this content
  // setting. We use an empty embedder since embargo doesn't care about it.
  std::set<GURL> embargoed_origins =
      GetPermissionDecisionAutoBlocker(browser_context)
          ->GetEmbargoedOrigins(
              static_cast<ContentSettingsType>(content_settings_type));
  ScopedJavaLocalRef<jstring> jembedder;
  for (const GURL& embargoed_origin : embargoed_origins) {
    if (base::Contains(seen_origins, embargoed_origin.spec()))
      continue;
    std::string embargoed_origin_pattern =
        ContentSettingsPattern::FromURLNoWildcard(embargoed_origin).ToString();
    Java_WebsitePreferenceBridge_addContentSettingExceptionToList(
        env, list, content_settings_type,
        ConvertUTF8ToJavaString(env, embargoed_origin_pattern), jembedder,
        CONTENT_SETTING_BLOCK,
        static_cast<int>(content_settings::ProviderType::kNone),
        /*isTemporary=*/false,
        /*expiration=*/0, /*is_embargoed=*/true);
  }
}

static jint JNI_WebsitePreferenceBridge_GetDefaultContentSetting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jbrowser_context_handle,
    int content_settings_type) {
  return GetHostContentSettingsMap(jbrowser_context_handle)
      ->GetDefaultContentSetting(
          static_cast<ContentSettingsType>(content_settings_type), nullptr);
}

static void JNI_WebsitePreferenceBridge_SetDefaultContentSetting(
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

static ScopedJavaLocalRef<jstring>
JNI_WebsitePreferenceBridge_ToDomainWildcardPattern(
    JNIEnv* env,
    const JavaParamRef<jstring>& pattern) {
  std::string pattern_string = ConvertJavaStringToUTF8(env, pattern);
  ContentSettingsPattern original_pattern =
      ContentSettingsPattern::FromString(pattern_string);
  ContentSettingsPattern domain_wildcard_pattern =
      ContentSettingsPattern::ToDomainWildcardPattern(original_pattern);
  std::string domain_wildcard_pattern_string =
      domain_wildcard_pattern.IsValid()
          ? domain_wildcard_pattern.ToString()
          : ContentSettingsPattern::ToHostOnlyPattern(original_pattern)
                .ToString();
  return ConvertUTF8ToJavaString(env, domain_wildcard_pattern_string);
}

static ScopedJavaLocalRef<jstring>
JNI_WebsitePreferenceBridge_ToHostOnlyPattern(
    JNIEnv* env,
    const JavaParamRef<jstring>& pattern) {
  std::string pattern_string = ConvertJavaStringToUTF8(env, pattern);
  ContentSettingsPattern host_only_pattern =
      ContentSettingsPattern::ToHostOnlyPattern(
          ContentSettingsPattern::FromString(pattern_string));
  return ConvertUTF8ToJavaString(env, host_only_pattern.ToString());
}
