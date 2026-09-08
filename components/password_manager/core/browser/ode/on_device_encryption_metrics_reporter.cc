// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/on_device_encryption_metrics_reporter.h"

#include <memory>
#include <utility>

#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_data_type_specific_metrics_reporter.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"

namespace password_manager {

OnDeviceEncryptionMetricsReporter::OnDeviceEncryptionMetricsReporter(
    std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker,
    std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker)
    : passkey_reporter_(kPasskeyOnDeviceEncryptionStateHistogram,
                        std::move(passkey_tracker)),
      password_reporter_(kPasswordOnDeviceEncryptionStateHistogram,
                         std::move(password_tracker)) {}

OnDeviceEncryptionMetricsReporter::~OnDeviceEncryptionMetricsReporter() =
    default;

void OnDeviceEncryptionMetricsReporter::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  passkey_reporter_.Shutdown();
  password_reporter_.Shutdown();
}

}  // namespace password_manager
