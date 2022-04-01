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
using ClassInfix = base::StrongAlias<struct ClassNameTag, std::string>;

// Records metrics for an asynchronous job or a series of jobs. The job is
// expected to have started when the PasswordStoreBackendMetricsRecorder
// instance is created. Latency is reported in RecordMetrics() under that
// assumption.
class PasswordStoreBackendMetricsRecorder {
 public:
  PasswordStoreBackendMetricsRecorder();
  explicit PasswordStoreBackendMetricsRecorder(ClassInfix class_name,
                                               MetricInfix metric_name);
  PasswordStoreBackendMetricsRecorder(PasswordStoreBackendMetricsRecorder&&);
  PasswordStoreBackendMetricsRecorder& operator=(
      PasswordStoreBackendMetricsRecorder&&);
  ~PasswordStoreBackendMetricsRecorder();

  // Records the following metrics:
  // - "PasswordManager.<class_infix_>.<metric_infix_>.Latency"
  // - "PasswordManager.<class_infix_>.<metric_infix_>.Success"
  // When |error| is specified, the following metrcis are recorded in
  // addition:
  // - "PasswordManager.<class_infix_>.APIError"
  // - "PasswordManager.<class_infix_>.ErrorCode"
  // - "PasswordManager.<class_infix_>.<metric_infix_>.APIError"
  // - "PasswordManager.<class_infix_>.<metric_infix_>.ErrorCode"
  void RecordMetrics(
      bool success,
      absl::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const;

 private:
  ClassInfix class_infix_;
  MetricInfix metric_infix_;
  base::Time start_ = base::Time::Now();
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_METRICS_RECORDER_H_