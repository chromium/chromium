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

// Include last. Requires declarations from includes above.
#include "components/supervised_user/android/jni_headers/ContentFiltersObserverBridge_jni.h"

namespace supervised_user {
std::unique_ptr<ContentFiltersObserverBridge>
ContentFiltersObserverBridge::Create(std::string_view setting_name,
                                     base::RepeatingClosure on_enabled,
                                     base::RepeatingClosure on_disabled) {
  return std::make_unique<ContentFiltersObserverBridge>(
      setting_name, on_enabled, on_disabled);
}

ContentFiltersObserverBridge::ContentFiltersObserverBridge(
    std::string_view setting_name,
    base::RepeatingClosure on_enabled,
    base::RepeatingClosure on_disabled)
    : setting_name_(setting_name),
      on_enabled_(on_enabled),
      on_disabled_(on_disabled) {
}

ContentFiltersObserverBridge::~ContentFiltersObserverBridge() {
  if (bridge_) {
    // Just in case when the owner forgot to call Shutdown().
    Shutdown();
    bridge_ = nullptr;
  }
}

void ContentFiltersObserverBridge::OnChange(JNIEnv* env, bool enabled) {
  LOG(INFO) << "ContentFiltersObserverBridge received onChange for setting "
            << setting_name_ << " with value "
            << (enabled ? "enabled" : "disabled");
  if (!base::FeatureList::IsEnabled(
          kPropagateDeviceContentFiltersToSupervisedUser)) {
    LOG(INFO)
        << "ContentFiltersObserverBridge change ignored: feature disabled";
    return;
  }

  if (enabled) {
    on_enabled_.Run();
  } else {
    on_disabled_.Run();
  }
}

void ContentFiltersObserverBridge::Init() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bridge_ = Java_ContentFiltersObserverBridge_Constructor(
      env, reinterpret_cast<jlong>(this),
      base::android::ConvertUTF8ToJavaString(env, setting_name_));
}

void ContentFiltersObserverBridge::Shutdown() {
  Java_ContentFiltersObserverBridge_destroy(
      base::android::AttachCurrentThread(), bridge_);
  bridge_ = nullptr;
}

bool ContentFiltersObserverBridge::IsEnabled() const {
  if (!base::FeatureList::IsEnabled(
          kPropagateDeviceContentFiltersToSupervisedUser)) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ContentFiltersObserverBridge_isEnabled(env, bridge_);
}

}  // namespace supervised_user
