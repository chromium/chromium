// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/smart_lock_metrics_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

SmartLockMetricsRecorder::SmartLockMetricsRecorder() = default;

SmartLockMetricsRecorder::~SmartLockMetricsRecorder() {}

void SmartLockMetricsRecorder::RecordSmartLockUnlockAuthMethodChoice(
    SmartLockAuthMethodChoice auth_method_choice) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.Unlock",
                            auth_method_choice);
}

void SmartLockMetricsRecorder::RecordSmartLockSignInAuthMethodChoice(
    SmartLockAuthMethodChoice auth_method_choice) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.SignIn",
                            auth_method_choice);
}

void SmartLockMetricsRecorder::RecordAuthResultUnlockSuccess(bool success) {
  UMA_HISTOGRAM_BOOLEAN("SmartLock.AuthResult.Unlock", success);
}

void SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(
    SmartLockAuthResultFailureReason failure_reason) {
  RecordAuthResultUnlockSuccess(false);
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthResult.Unlock.Failure",
                            failure_reason);
}

void SmartLockMetricsRecorder::RecordAuthResultSignInSuccess(bool success) {
  UMA_HISTOGRAM_BOOLEAN("SmartLock.AuthResult.SignIn", success);
}

void SmartLockMetricsRecorder::RecordAuthResultSignInFailure(
    SmartLockAuthResultFailureReason failure_reason) {
  RecordAuthResultSignInSuccess(false);
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthResult.SignIn.Failure",
                            failure_reason);
}

void SmartLockMetricsRecorder::RecordAuthMethodChoiceUnlockPasswordState(
    SmartLockAuthEventPasswordState password_state) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.Unlock.PasswordState",
                            password_state);
}

void SmartLockMetricsRecorder::RecordAuthMethodChoiceSignInPasswordState(
    SmartLockAuthEventPasswordState password_state) {
  UMA_HISTOGRAM_ENUMERATION("SmartLock.AuthMethodChoice.SignIn.PasswordState",
                            password_state);
}
