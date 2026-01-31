// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/installable/installed_webapp_bridge.h"

#include <utility>
#include <variant>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/memory/safety_checks.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/InstalledWebappBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;

namespace {

// TODO(crbug.com/391248369): We can just write
//
// using PermissionCallbackJava = base::OnceCallback<
//   void(PermissionDecision setting,
//        bool is_final_decision)>;
//
// and remove ADVANCED_MEMORY_SAFETY_CHECKS() once https://crbug.com/391248369
// is fixed.
class PermissionCallbackWithAMSC
    : public base::OnceCallback<void(PermissionDecision setting,
                                     bool is_final_decision)> {
  ADVANCED_MEMORY_SAFETY_CHECKS();
};

}  // namespace

static void JNI_InstalledWebappBridge_NotifyPermissionsChange(
    JNIEnv* env,
    int64_t j_provider,
    int type_int) {
  ContentSettingsType type = static_cast<ContentSettingsType>(type_int);
  DCHECK(IsKnownEnumValue(type));
  InstalledWebappProvider* provider =
    reinterpret_cast<InstalledWebappProvider*>(j_provider);
  provider->Notify(type);
}

static void JNI_InstalledWebappBridge_RunPermissionCallback(
    JNIEnv* env,
    int64_t callback_ptr,
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
      base::android::AttachCurrentThread(), (int64_t)provider);
}

void InstalledWebappBridge::DecidePermission(ContentSettingsType type,
                                             const GURL& origin_url,
                                             const GURL& last_committed_url,
                                             PermissionCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // TODO(crbug.com/470038595): IWAs don't fully support
  // GEOLOCATION_WITH_OPTIONS yet, so we hardcode precise accuracy as the
  // selected value here.
  PromptOptions prompt_options =
      (type == ContentSettingsType::GEOLOCATION_WITH_OPTIONS)
          ? PromptOptions(GeolocationPromptOptions{
                .selected_accuracy = GeolocationAccuracy::kPrecise})
          : std::monostate();

  // Transfers the ownership of the callback to the Java callback. The Java
  // callback is guaranteed to be called unless the user never replies to the
  // dialog, but as the dialog is modal, the only other thing the user can do
  // is quit Chrome which will also free the pointer. The callback pointer will
  // be destroyed in RunPermissionCallback.
  PermissionCallbackWithAMSC* callback_ptr =
      new PermissionCallbackWithAMSC(base::BindOnce(
          [](PermissionCallback callback, const PromptOptions& prompt_options,
             PermissionDecision decision, bool is_final_decision) {
            std::move(callback).Run(permissions::PermissionPromptDecision{
                .overall_decision = decision,
                .prompt_options = prompt_options,
                .is_final = is_final_decision});
          },
          std::move(callback), prompt_options));

  Java_InstalledWebappBridge_decidePermission(
      env, static_cast<int>(type), origin_url.spec(), last_committed_url.spec(),
      reinterpret_cast<int64_t>(callback_ptr));
}

DEFINE_JNI(InstalledWebappBridge)
