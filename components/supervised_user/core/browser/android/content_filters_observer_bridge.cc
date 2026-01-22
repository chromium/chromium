// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"

#include <string_view>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

// Include last. Requires declarations from includes above.
#include "components/supervised_user/android/jni_headers/ContentFiltersObserverBridge_jni.h"

namespace supervised_user {

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
  enabled_ = enabled;
  NotifyObservers();
}

void ContentFiltersObserverBridge::NotifyObservers() {
  observer_list_.Notify(&Observer::OnContentFiltersObserverChanged);
}

void ContentFiltersObserverBridge::Init() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bridge_ = Java_ContentFiltersObserverBridge_Constructor(
      env, reinterpret_cast<int64_t>(this),
      base::android::ConvertUTF8ToJavaString(env, setting_name_));
}

void ContentFiltersObserverBridge::Shutdown() {
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
