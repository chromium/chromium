// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/installable/installed_webapp_bridge.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/memory/safety_checks.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_decision.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/InstalledWebappBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;

namespace {

// TODO(crbug.com/391248369): Remove this class once the bug gets fixed, and
// replace the usages of PermissionCallbackWithAMSC with
// InstalledWebappBridge::PermissionCallback.
//
// This class `PermissionCallbackWithAMSC` is needed because
// `InstalledWebappBridge::PermissionCallback` is just a type alias to
// `base::OnceCallback` and we don't want to burden all base::OnceCallbacks
// just because of this UaF issue. This class allows us to apply the
// `ADVANCED_MEMORY_SAFETY_CHECKS` macro to PermissionCallback only.
class PermissionCallbackWithAMSC
    : public InstalledWebappBridge::PermissionCallback {
  ADVANCED_MEMORY_SAFETY_CHECKS();
};

}  // namespace

static void JNI_InstalledWebappBridge_NotifyPermissionsChange(JNIEnv* env,
                                                              jlong j_provider,
                                                              int type_int) {
  ContentSettingsType type = static_cast<ContentSettingsType>(type_int);
  DCHECK(IsKnownEnumValue(type));
  InstalledWebappProvider* provider =
    reinterpret_cast<InstalledWebappProvider*>(j_provider);
  provider->Notify(type);
}

static void JNI_InstalledWebappBridge_RunPermissionCallback(JNIEnv* env,
                                                            jlong callback_ptr,
                                                            int setting) {
  DCHECK_LE(setting, static_cast<int>(PermissionDecision::kMaxValue));
  auto* callback = reinterpret_cast<PermissionCallbackWithAMSC*>(callback_ptr);
  std::move(*callback).Run(
      static_cast<PermissionDecision>(static_cast<PermissionDecision>(setting)),
      /*is_final_decision=*/true);
  delete callback;
}

InstalledWebappProvider::RuleList
InstalledWebappBridge::GetInstalledWebappPermissions(ContentSettingsType type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_permissions =
      Java_InstalledWebappBridge_getPermissions(env, static_cast<int>(type));

  InstalledWebappProvider::RuleList rules;
  for (auto j_permission : j_permissions.ReadElements<jobject>()) {
    GURL origin(
        Java_InstalledWebappBridge_getOriginFromPermission(env, j_permission));
    ContentSetting setting = IntToContentSetting(
        Java_InstalledWebappBridge_getSettingFromPermission(env, j_permission));
    PermissionSetting permission_setting = setting;
    if (type == ContentSettingsType::GEOLOCATION_WITH_OPTIONS) {
      permission_setting =
          GeolocationSetting{content_settings::ToPermissionOption(setting),
                             content_settings::ToPermissionOption(setting)};
    }
    rules.emplace_back(origin, permission_setting);
  }

  return rules;
}

void InstalledWebappBridge::SetProviderInstance(
    InstalledWebappProvider *provider) {
  Java_InstalledWebappBridge_setInstalledWebappProvider(
      base::android::AttachCurrentThread(), (jlong) provider);
}

void InstalledWebappBridge::DecidePermission(ContentSettingsType type,
                                             const GURL& origin_url,
                                             const GURL& last_committed_url,
                                             PermissionCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Transfers the ownership of the callback to the Java callback. The Java
  // callback is guaranteed to be called unless the user never replies to the
  // dialog, but as the dialog is modal, the only other thing the user can do
  // is quit Chrome which will also free the pointer. The callback pointer will
  // be destroyed in RunPermissionCallback.
  auto* callback_ptr = new PermissionCallbackWithAMSC(std::move(callback));

  Java_InstalledWebappBridge_decidePermission(
      env, static_cast<int>(type), origin_url.spec(), last_committed_url.spec(),
      reinterpret_cast<jlong>(callback_ptr));
}

DEFINE_JNI(InstalledWebappBridge)
