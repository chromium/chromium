// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

// Include last. Requires declarations from includes above.
#include "components/supervised_user/android/jni_headers/ContentFiltersObserverBridge_jni.h"

namespace supervised_user {

namespace {
const char kSupervisionConflictHistogramName[] =
    "SupervisedUsers.FamilyLinkSupervisionConflict";
enum class SupervisionHasConflict : int {
  kNoConflict = 0,
  kHasConflict = 1,
  kMaxValue = kHasConflict,
};

// Each of the content filters have their own kill switch. This function
// returns true if the feature is enabled for the given setting.
bool IsFeatureEnabledForSetting(std::string_view setting_name) {
  if (!UseLocalSupervision()) {
    return false;
  }

  if (setting_name == kBrowserContentFiltersSettingName) {
    return base::FeatureList::IsEnabled(
        kSupervisedUserBrowserContentFiltersKillSwitch);
  } else if (setting_name == kSearchContentFiltersSettingName) {
    return base::FeatureList::IsEnabled(
        kSupervisedUserSearchContentFiltersKillSwitch);
  } else {
    NOTREACHED() << "Unsupported setting name: " << setting_name;
  }
}
}  // namespace

std::unique_ptr<ContentFiltersObserverBridge>
ContentFiltersObserverBridge::Create(
    std::string_view setting_name,
    base::RepeatingClosure on_enabled,
    base::RepeatingClosure on_disabled,
    base::RepeatingCallback<bool()> is_subject_to_parental_controls) {
  return std::make_unique<ContentFiltersObserverBridge>(
      setting_name, on_enabled, on_disabled, is_subject_to_parental_controls);
}

ContentFiltersObserverBridge::ContentFiltersObserverBridge(
    std::string_view setting_name,
    base::RepeatingClosure on_enabled,
    base::RepeatingClosure on_disabled,
    base::RepeatingCallback<bool()> is_subject_to_parental_controls)
    : setting_name_(setting_name),
      on_enabled_(on_enabled),
      on_disabled_(on_disabled),
      is_subject_to_parental_controls_(is_subject_to_parental_controls) {}

ContentFiltersObserverBridge::~ContentFiltersObserverBridge() {
  if (bridge_) {
    // Just in case when the owner forgot to call Shutdown().
    Shutdown();
    bridge_ = nullptr;
  }
}

void ContentFiltersObserverBridge::OnChange(JNIEnv* env, bool enabled) {
  // Warning: callsites can pass env=nullptr. Update them before utilizing *env.

  LOG(INFO) << "ContentFiltersObserverBridge received onChange for setting "
            << setting_name_ << " with value "
            << (enabled ? "enabled" : "disabled");
  if (!IsFeatureEnabledForSetting(setting_name_)) {
    LOG(INFO)
        << "ContentFiltersObserverBridge change ignored: feature disabled";
    return;
  }

  // This prevents the content filters from being enabled for family link
  // accounts.
  if (base::FeatureList::IsEnabled(
          kSupervisedUserOverrideLocalSupervisionForFamilyLinkAccounts) &&
      is_subject_to_parental_controls_.Run() && enabled) {
    base::UmaHistogramEnumeration(kSupervisionConflictHistogramName,
                                  SupervisionHasConflict::kHasConflict);
    LOG(INFO)
        << "ContentFiltersObserverBridge change ignored: family link user";
    return;
  }

  enabled_ = enabled;
  if (enabled) {
    on_enabled_.Run();
  } else {
    on_disabled_.Run();
  }
}

void ContentFiltersObserverBridge::Init() {
  if (!IsFeatureEnabledForSetting(setting_name_)) {
    LOG(INFO)
        << "ContentFiltersObserverBridge not initialized: feature disabled";
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  bridge_ = Java_ContentFiltersObserverBridge_Constructor(
      env, reinterpret_cast<jlong>(this),
      base::android::ConvertUTF8ToJavaString(env, setting_name_));
}

void ContentFiltersObserverBridge::Shutdown() {
  if (!IsFeatureEnabledForSetting(setting_name_)) {
    LOG(INFO) << "ContentFiltersObserverBridge not shutdown: feature disabled";
    return;
  }

  Java_ContentFiltersObserverBridge_destroy(
      base::android::AttachCurrentThread(), bridge_);
  bridge_ = nullptr;
}

bool ContentFiltersObserverBridge::IsEnabled() const {
  return enabled_;
}

void ContentFiltersObserverBridge::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

}  // namespace supervised_user

DEFINE_JNI(ContentFiltersObserverBridge)
