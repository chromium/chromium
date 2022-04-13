// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_

#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/strcat.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  PasswordStoreBackendMetricsRecorder();
  explicit PasswordStoreBackendMetricsRecorder(BackendInfix backend_name,
                                               MetricInfix metric_name);
  PasswordStoreBackendMetricsRecorder(PasswordStoreBackendMetricsRecorder&&);
  PasswordStoreBackendMetricsRecorder& operator=(
      PasswordStoreBackendMetricsRecorder&&);
  ~PasswordStoreBackendMetricsRecorder();

  // Records the following metrics:
  // - "PasswordManager.PasswordStore<backend_infix_>.<metric_infix_>.Latency"
  // - "PasswordManager.PasswordStore<backend_infix_>.<metric_infix_>.Success"
  // - "PasswordManager.PasswordStoreBackend.<metric_infix_>.Latency"
  // - "PasswordManager.PasswordStoreBackend.<metric_infix_>.Success"
  // In case of Android backend, when |error| is specified, the following
  // metrcis are recorded in addition:
  // - "PasswordManager.PasswordStoreAndroidBackend.APIError"
  // - "PasswordManager.PasswordStoreAndroidBackend.ErrorCode"
  // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.APIError"
  // - "PasswordManager.PasswordStoreAndroidBackend.<metric_infix_>.ErrorCode"
  void RecordMetrics(
      bool success,
      absl::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const;

 private:
  BackendInfix backend_infix_;
  MetricInfix metric_infix_;
  base::Time start_ = base::Time::Now();
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_
