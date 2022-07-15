// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_

#include <string>

#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {

using ErrorFromPasswordStoreOrAndroidBackend =
    absl::variant<PasswordStoreBackendError, AndroidBackendError>;

using MetricInfix = base::StrongAlias<struct MetricNameTag, std::string>;
using BackendInfix = base::StrongAlias<struct BackendNameTag, std::string>;

// Records metrics for an asynchronous job or a series of jobs. The job is
// expected to have started when the PasswordStoreBackendMetricsRecorder
// instance is created. Latency is reported in RecordMetrics() under that
// assumption.
class PasswordStoreBackendMetricsRecorder {
 public:
  enum class SuccessStatus { kSuccess, kError, kCancelled };

  PasswordStoreBackendMetricsRecorder();
  explicit PasswordStoreBackendMetricsRecorder(BackendInfix backend_name,
                                               MetricInfix metric_name);
  PasswordStoreBackendMetricsRecorder(PasswordStoreBackendMetricsRecorder&&);
  PasswordStoreBackendMetricsRecorder& operator=(
      PasswordStoreBackendMetricsRecorder&&);
  ~PasswordStoreBackendMetricsRecorder();

  // Records metrics from `RecordSuccess`.
  // Records metrics from `RecordLatency`.
  // Records metrics from `RecordErrorCode` if `error` is specified.
  void RecordMetrics(
      SuccessStatus success_status,
      absl::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const;

  // Records success, error and API error metrics from clients unenrolled from
  // the UPM experiment after experiencing errors in the android backend.
  void RecordMetricsForUnenrolledClients(
      const absl::optional<AndroidBackendError>& error) const;

  // Returns the delta between creating this recorder and calling this method.
  base::TimeDelta GetElapsedTimeSinceCreation() const;

 private:
  // Records the following metrics:
  // - "PasswordManager.PasswordStore<backend_infix_>.<metric_infix_>.Success"
  // - "PasswordManager.PasswordStoreBackend.<metric_infix_>.Success"
  void RecordSuccess(SuccessStatus success_status) const;

  // Records metrics from `RecordApiErrorCode` if `backend_error`
  // requires it. Additionally records the following metrics:
  // - "PasswordManager.PasswordStoreAndroidBackend.ErrorCode"
  // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.ErrorCode"
  void RecordErrorCode(const AndroidBackendError& backend_error) const;

  // Records the following metrics:
  // - "PasswordManager.PasswordStore<backend_infix_>.<metric_infix_>.Latency"
  // - "PasswordManager.PasswordStoreBackend.<metric_infix_>.Latency"
  void RecordLatency() const;

  // Records the following metrics:
  // - "PasswordManager.PasswordStoreAndroidBackend.APIError"
  // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.APIError"
  void RecordApiErrorCode(int api_error_code) const;

  std::string BuildMetricName(base::StringPiece suffix) const;
  std::string BuildOverallMetricName(base::StringPiece suffix) const;

  BackendInfix backend_infix_;
  MetricInfix metric_infix_;
  base::Time start_ = base::Time::Now();
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_
