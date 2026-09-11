// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"

class PrefRegistrySimple;
class PrefService;

namespace password_manager {

inline constexpr char kPasskeyOnDeviceEncryptionStateHistogram[] =
    "PasswordManager.OnDeviceEncryptionState.Passkeys";
inline constexpr char kPasswordOnDeviceEncryptionStateHistogram[] =
    "PasswordManager.OnDeviceEncryptionState.Passwords";

// KeyedService that manages on-device encryption metrics reporters for
// passwords and passkeys.
class OnDeviceEncryptionMetricsReporter : public KeyedService {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  OnDeviceEncryptionMetricsReporter(
      std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker,
      std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker,
      PrefService& pref_service);

  OnDeviceEncryptionMetricsReporter(const OnDeviceEncryptionMetricsReporter&) =
      delete;
  OnDeviceEncryptionMetricsReporter& operator=(
      const OnDeviceEncryptionMetricsReporter&) = delete;

  ~OnDeviceEncryptionMetricsReporter() override;

  // KeyedService:
  void Shutdown() override;

 private:
  OnDeviceEncryptionDataTypeSpecificMetricsReporter passkey_reporter_;
  OnDeviceEncryptionDataTypeSpecificMetricsReporter password_reporter_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_ON_DEVICE_ENCRYPTION_METRICS_REPORTER_H_
