// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

// Include last. Requires declarations from includes above.
#include "components/supervised_user/android/jni_headers/ContentFiltersObserverBridge_jni.h"

namespace supervised_user {

namespace {
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

ContentFiltersObserverBridge::ContentFiltersObserverBridge(
    std::string_view setting_name)
    : setting_name_(setting_name) {}

ContentFiltersObserverBridge::~ContentFiltersObserverBridge() {
  if (bridge_) {
    // Just in case when the owner forgot to call Shutdown().
    Shutdown();
    bridge_ = nullptr;
  }
}

void ContentFiltersObserverBridge::SetEnabledForTesting(bool enabled) {
  SetEnabled(enabled);
}

void ContentFiltersObserverBridge::OnChange(JNIEnv* env, bool enabled) {
  DVLOG(1) << "ContentFiltersObserverBridge received onChange for setting "
           << setting_name_ << " with value "
           << (enabled ? "enabled" : "disabled");
  SetEnabled(enabled);
}

void ContentFiltersObserverBridge::SetEnabled(bool enabled) {
  if (!IsFeatureEnabledForSetting(setting_name_)) {
    DVLOG(1) << "ContentFiltersObserverBridge change ignored: feature disabled";
    return;
  }

  enabled_ = enabled;
  NotifyObservers();
}

void ContentFiltersObserverBridge::NotifyObservers() {
  if (enabled_) {
    observer_list_.Notify(&Observer::OnContentFiltersObserverEnabled,
                          setting_name_);
  } else {
    observer_list_.Notify(&Observer::OnContentFiltersObserverDisabled,
                          setting_name_);
  }
}

void ContentFiltersObserverBridge::Init() {
  if (!IsFeatureEnabledForSetting(setting_name_)) {
    DVLOG(1)
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
    DVLOG(1) << "ContentFiltersObserverBridge not shutdown: feature disabled";
    return;
  }

  Java_ContentFiltersObserverBridge_destroy(
      base::android::AttachCurrentThread(), bridge_);
  bridge_ = nullptr;
}

bool ContentFiltersObserverBridge::IsEnabled() const {
  return enabled_;
}

void ContentFiltersObserverBridge::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ContentFiltersObserverBridge::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
}  // namespace supervised_user

DEFINE_JNI(ContentFiltersObserverBridge)
