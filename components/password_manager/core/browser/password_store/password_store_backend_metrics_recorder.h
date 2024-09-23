// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {

using ErrorFromPasswordStoreOrAndroidBackend =
    absl::variant<PasswordStoreBackendError, AndroidBackendError>;
// TODO(b/322972811): Use the metrics recorder only for Android.
using MethodName = base::StrongAlias<struct MetricNameTag, std::string>;
using BackendInfix = base::StrongAlias<struct BackendNameTag, std::string>;

// Records metrics for an asynchronous job or a series of jobs. The job is
// expected to have started when the PasswordStoreBackendMetricsRecorder
// instance is created. Latency is reported in RecordMetrics() under that
// assumption.
class PasswordStoreBackendMetricsRecorder {
 public:
  enum class SuccessStatus {
    kSuccess,
    kError,
    kCancelledTimeout,
    kCancelledPwdSyncStateChanged,
  };

  enum class PasswordStoreAndroidBackendType {
    kLocal,
    kAccount,
    kNone,
  };

  PasswordStoreBackendMetricsRecorder();
  // Constructs a new recorder and immediately calls `RecordRequestStatus()` to
  // indicate a new request is started.
  PasswordStoreBackendMetricsRecorder(
      BackendInfix backend_name,
      MethodName method_name,
      PasswordStoreAndroidBackendType store_type);
  PasswordStoreBackendMetricsRecorder(PasswordStoreBackendMetricsRecorder&&);
  PasswordStoreBackendMetricsRecorder& operator=(
      PasswordStoreBackendMetricsRecorder&&);
  ~PasswordStoreBackendMetricsRecorder();

  // Records metrics from `RecordSuccess`.
  // Records metrics from `RecordLatency`.
  // Records metrics from `RecordErrorCode` if `error` is specified.
  void RecordMetrics(
      SuccessStatus success_status,
      std::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const;

  // Returns the delta between creating this recorder and calling this method.
  base::TimeDelta GetElapsedTimeSinceCreation() const;

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class StoreBackendRequestStatus {
    kRequestIssued = 0,
    kTimeout = 1,
    kCompleted = 2,
    // Recorded when a request was cancelled because the sync/sign-in status
    // changed, causing the destination storage of the request to become
    // invalid.
    kCancelledPwdSyncStateChanged = 3,
    kMaxValue = kCancelledPwdSyncStateChanged,
  };

  // Records a broad status for an ongoing request:
  // - "PasswordManager.PasswordStoreBackend.<method_name_>"
  // - "PasswordManager.PasswordStore<backend_infix_>.<method_name_>"
  // Records additionally if the store infix is provided:
  // -
  // "PasswordManager.PasswordStore<backend_infix_>.<store
  // infix>.<method_name_>"
  void RecordRequestStatus(StoreBackendRequestStatus request_status) const;

  // Records the following metrics:
  // - "PasswordManager.PasswordStore<backend_infix_>.<method_name_>.Success"
  // - "PasswordManager.PasswordStoreBackend.<method_name_>.Success"
  // Records additionally if the store infix is provided:
  // -
  // "PasswordManager.PasswordStore<backend_infix_>.<store
  // infix>.<method_name_>.Success"
  void RecordSuccess(SuccessStatus success_status) const;

  // Records metrics from `RecordApiErrorCode` if `backend_error`
  // requires it. Additionally records the following metrics:
  // - "PasswordManager.PasswordStoreAndroidBackend.ErrorCode"
  // - "PasswordManager.PasswordStoreAndroidBackend.<method_name_>.ErrorCode"
  // Records additionally if the store infix is provided:
  // - "PasswordManager.PasswordStoreAndroidBackend.<store infix>.ErrorCode"
  // -
  // "PasswordManager.PasswordStoreAndroidBackend.<store
  // infix>.<method_name_>.ErrorCode"
  void RecordErrorCode(const AndroidBackendError& backend_error) const;

  // Records the following metrics:
  // - "PasswordManager.PasswordStore<backend_infix_>.<method_name_>.Latency"
  // - "PasswordManager.PasswordStoreBackend.<method_name_>.Latency"
  // Records additionally if the store infix is provided:
  // -
  // "PasswordManager.PasswordStore<backend_infix_>.<store
  // infix>.<method_name_>.Latency"
  void RecordLatency() const;

  // Records the following metrics:
  // - "PasswordManager.PasswordStoreAndroidBackend.APIError"
  // - "PasswordManager.PasswordStoreAndroidBackend.<method_name_>.APIError"
  // Records additionally if the store infix is provided:
  // - "PasswordManager.PasswordStoreAndroidBackend.<store infix>.APIError"
  // -
  // "PasswordManager.PasswordStoreAndroidBackend.<store
  // infix>.<method_name_>.APIError"
  void RecordApiErrorCode(int api_error_code) const;

  // Records the following metrics:
  // - "PasswordManager.PasswordStoreAndroidBackend.ConnectionResultCode"
  // - "PasswordManager.PasswordStoreAndroidBackend.<method_name_>
  //        .ConnectionResultCode"
  // Records additionally if the store infix is provided:
  // -
  // "PasswordManager.PasswordStoreAndroidBackend.<store
  // infix>.<method_name_>.ConnectionResultCode"
  void RecordConnectionResultCode(int connection_result_code) const;

  std::string GetStoreInfix() const;

  BackendInfix backend_infix_;
  MethodName method_name_;
  PasswordStoreAndroidBackendType store_type_;
  base::Time start_ = base::Time::Now();
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_
