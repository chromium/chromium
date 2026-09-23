// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_BROWSER_ASSISTED_LOGIN_TYPE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_BROWSER_ASSISTED_LOGIN_TYPE_H_

namespace password_manager::metrics_util {

// This enum describes the type of logins assisted by the browser. e.g. via
// passwords, passkeys or federation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(BrowserAssistedLoginType)
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.password_manager
enum class BrowserAssistedLoginType {
  kFedCmPassive = 0,
  kFedCmActive = 1,
  kNonFedCmOAuth = 2,
  kUnknown = 3,
  kPasswordFullyAssisted = 4,
  kPasswordPartiallyAssisted = 5,
  kPasswordManuallyEntered = 6,
  kPasswordNeitherManuallyEnteredNorGPMAssisted = 7,
  kPasskeyStoredInGPM = 8,
  kPasskeyStoredInWindowsHello = 9,
  kPasskeyStoredInICloudKeychain = 10,
  kPasskeyStoredInChromeProfile = 11,
  kPasskeyHybrid = 12,
  kPasskeySecurityKey = 13,
  kPasskeyHybridOrSecurityKey = 14,
  kPasskeyUnknown = 15,
  kPasskeyStoredInGPMFacilitatedThroughIOSUI = 16,

  kMaxValue = kPasskeyStoredInGPMFacilitatedThroughIOSUI,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:BrowserAssistedLoginType)

}  // namespace password_manager::metrics_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_BROWSER_ASSISTED_LOGIN_TYPE_H_
