// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_REAUTH_DEVICE_REAUTH_METRICS_UTIL_H_
#define COMPONENTS_DEVICE_REAUTH_DEVICE_REAUTH_METRICS_UTIL_H_

namespace device_reauth {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// The place where the device reauthentication flow is requested from.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class DeviceAuthSource {
  kPasswordManager = 0,
  kAutofill = 1,
  kIncognito = 2,
  kDeviceLockPage = 3,
  kSettingsBatchUpload = 4,
  kMaxValue = kSettingsBatchUpload,
};

// The result of the device reauthentication attempt.
// Needs to stay in sync with "DeviceReauth.ReauthResult" in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class ReauthResult {
  kSuccess = 0,
  kFailure = 1,
  kSkipped = 2,
  kMaxValue = kSkipped,
};

}  // namespace device_reauth

#endif  // COMPONENTS_DEVICE_REAUTH_DEVICE_REAUTH_METRICS_UTIL_H_
