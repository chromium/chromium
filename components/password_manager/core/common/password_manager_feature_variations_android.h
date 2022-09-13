// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURE_VARIATIONS_ANDROID_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURE_VARIATIONS_ANDROID_H_

namespace password_manager::features {

// This enum supports enabling specific parts of the Unified Password Manager
// via command line flag and configuration alike.
// Do not reassign or delete indices but mark them deprecated since they are
// used to parse the enum in java. Keep the order consistent with
// `kUpmExperimentStageOption` below, with java helpers, and with
// `kUnifiedPasswordManagerAndroidVariations` in about_flags.cc
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class UpmExperimentVariation {
  // Default variation. Make the Android backend default for syncing users for
  // all requests. Uses updated UI and no shadow traffic.
  kEnableForSyncingUsers = 0,

  // Read-only shadow traffic to Android backend for syncing users. The built-in
  // backend remains the default for all requests. Uses legacy UI.
  kShadowSyncingUsers = 1,

  // Make the Android backend default for syncing users for all requests. Uses
  // legacy UI but no shadow traffic.
  kEnableOnlyBackendForSyncingUsers = 2,

  // Make the Android backend default for syncing and non-syncing users for all
  // requests. Uses updated UI and no shadow traffic.
  kEnableForAllUsers = 3,
};

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_FEATURE_VARIATIONS_ANDROID_H_
