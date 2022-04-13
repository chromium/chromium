// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"

namespace password_manager {

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder() =
    default;

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder(
    BackendInfix backend_infix,
    MetricInfix metric_infix)
    : backend_infix_(std::move(backend_infix)),
      metric_infix_(std::move(metric_infix)) {}

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder(
    PasswordStoreBackendMetricsRecorder&&) = default;

PasswordStoreBackendMetricsRecorder& PasswordStoreBackendMetricsRecorder::
    PasswordStoreBackendMetricsRecorder::operator=(
        PasswordStoreBackendMetricsRecorder&&) = default;

PasswordStoreBackendMetricsRecorder::~PasswordStoreBackendMetricsRecorder() =
    default;

void PasswordStoreBackendMetricsRecorder::RecordMetrics(
    bool success,
    absl::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const {
  auto BuildMetricName = [this](base::StringPiece suffix) {
    return base::StrCat({"PasswordManager.PasswordStore", *backend_infix_, ".",
                         *metric_infix_, ".", suffix});
  };
  auto BuildOverallMetricName = [this](base::StringPiece suffix) {
    return base::StrCat(
        {"PasswordManager.PasswordStoreBackend.", *metric_infix_, ".", suffix});
  };
  base::TimeDelta duration = base::Time::Now() - start_;
  base::UmaHistogramMediumTimes(BuildMetricName("Latency"), duration);
  base::UmaHistogramBoolean(BuildMetricName("Success"), success);
  base::UmaHistogramMediumTimes(BuildOverallMetricName("Latency"), duration);
  base::UmaHistogramBoolean(BuildOverallMetricName("Success"), success);
  if (!error.has_value())
    return;

  DCHECK(!success);
  ErrorFromPasswordStoreOrAndroidBackend error_variant =
      std::move(error.value());
  // In case of AndroidBackend error, we report additional metrics.
  if (absl::holds_alternative<AndroidBackendError>(error_variant)) {
    AndroidBackendError backend_error = std::move(absl::get<1>(error_variant));
    base::UmaHistogramEnumeration(
        "PasswordManager.PasswordStoreAndroidBackend.ErrorCode",
        backend_error.type);
    base::UmaHistogramEnumeration(BuildMetricName("ErrorCode"),
                                  backend_error.type);
    if (backend_error.type == AndroidBackendErrorType::kExternalError) {
      DCHECK(backend_error.api_error_code.has_value());
      base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
          "PasswordManager.PasswordStoreAndroidBackend.APIError",
          base::HistogramBase::kUmaTargetedHistogramFlag);
      histogram->Add(backend_error.api_error_code.value());
      histogram = base::SparseHistogram::FactoryGet(
          BuildMetricName("APIError"),
          base::HistogramBase::kUmaTargetedHistogramFlag);
      histogram->Add(backend_error.api_error_code.value());
    }
  }
}

}  // namespace password_manager
