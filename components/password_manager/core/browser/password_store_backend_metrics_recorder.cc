// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace password_manager {

namespace {
constexpr char kMetricPrefix[] = "PasswordManager.PasswordStore";

bool HasRunToCompletion(
    PasswordStoreBackendMetricsRecorder::SuccessStatus success_status) {
  switch (success_status) {
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kSuccess:
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kError:
      return true;
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kCancelled:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder() =
    default;

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder(
    BackendInfix backend_infix,
    MetricInfix metric_infix)
    : backend_infix_(std::move(backend_infix)),
      metric_infix_(std::move(metric_infix)) {
  RecordRequestStatus(StoreBackendRequestStatus::kRequestIssued);
}

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder(
    PasswordStoreBackendMetricsRecorder&&) = default;

PasswordStoreBackendMetricsRecorder& PasswordStoreBackendMetricsRecorder::
    PasswordStoreBackendMetricsRecorder::operator=(
        PasswordStoreBackendMetricsRecorder&&) = default;

PasswordStoreBackendMetricsRecorder::~PasswordStoreBackendMetricsRecorder() =
    default;

void PasswordStoreBackendMetricsRecorder::RecordMetrics(
    SuccessStatus success_status,
    absl::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const {
  RecordSuccess(success_status);
  if (HasRunToCompletion(success_status)) {
    RecordLatency();
    RecordRequestStatus(StoreBackendRequestStatus::kCompleted);
  } else {
    RecordRequestStatus(StoreBackendRequestStatus::kTimeout);
  }
  if (error.has_value()) {
    DCHECK_NE(success_status, SuccessStatus::kSuccess);
    if (absl::holds_alternative<AndroidBackendError>(error.value())) {
      RecordErrorCode(std::move(absl::get<1>(error.value())));
    }
  }
}

base::TimeDelta
PasswordStoreBackendMetricsRecorder::GetElapsedTimeSinceCreation() const {
  return base::Time::Now() - start_;
}

void PasswordStoreBackendMetricsRecorder::RecordRequestStatus(
    StoreBackendRequestStatus request_status) const {
  base::UmaHistogramEnumeration(GetBackendMetricName(), request_status);
  base::UmaHistogramEnumeration(GetOverallMetricName(), request_status);
}

void PasswordStoreBackendMetricsRecorder::RecordSuccess(
    SuccessStatus success_status) const {
  base::UmaHistogramBoolean(BuildMetricName("Success"),
                            success_status == SuccessStatus::kSuccess);
  base::UmaHistogramBoolean(BuildOverallMetricName("Success"),
                            success_status == SuccessStatus::kSuccess);
}

void PasswordStoreBackendMetricsRecorder::RecordErrorCode(
    const AndroidBackendError& backend_error) const {
  base::UmaHistogramEnumeration(
      base::StrCat({kMetricPrefix, "AndroidBackend.ErrorCode"}),
      backend_error.type);
  base::UmaHistogramEnumeration(BuildMetricName("ErrorCode"),
                                backend_error.type);
  if (backend_error.type == AndroidBackendErrorType::kExternalError) {
    DCHECK(backend_error.api_error_code.has_value());
    RecordApiErrorCode(backend_error.api_error_code.value());
    LOG(ERROR) << "Password Manager API call for " << metric_infix_
               << " failed with error code: "
               << backend_error.api_error_code.value();
  }
  if (backend_error.connection_result_code.has_value()) {
    RecordConnectionResultCode(backend_error.connection_result_code.value());
  }
}

void PasswordStoreBackendMetricsRecorder::RecordLatency() const {
  base::TimeDelta duration = GetElapsedTimeSinceCreation();
  base::UmaHistogramMediumTimes(BuildMetricName("Latency"), duration);
  base::UmaHistogramMediumTimes(BuildOverallMetricName("Latency"), duration);
}

void PasswordStoreBackendMetricsRecorder::RecordApiErrorCode(
    int api_error_code) const {
  base::UmaHistogramSparse(
      base::StrCat({kMetricPrefix, "AndroidBackend.APIError"}), api_error_code);
  base::UmaHistogramSparse(BuildMetricName("APIError"), api_error_code);
}

void PasswordStoreBackendMetricsRecorder::RecordConnectionResultCode(
    int connection_result_code) const {
  base::UmaHistogramSparse(
      base::StrCat({kMetricPrefix, "AndroidBackend.ConnectionResultCode"}),
      connection_result_code);
  base::UmaHistogramSparse(BuildMetricName("ConnectionResultCode"),
                           connection_result_code);
}

std::string PasswordStoreBackendMetricsRecorder::GetBackendMetricName() const {
  return base::StrCat({kMetricPrefix, *backend_infix_, ".", *metric_infix_});
}

std::string PasswordStoreBackendMetricsRecorder::BuildMetricName(
    base::StringPiece suffix) const {
  return base::StrCat({GetBackendMetricName(), ".", suffix});
}

std::string PasswordStoreBackendMetricsRecorder::GetOverallMetricName() const {
  return base::StrCat({kMetricPrefix, "Backend.", *metric_infix_});
}

std::string PasswordStoreBackendMetricsRecorder::BuildOverallMetricName(
    base::StringPiece suffix) const {
  return base::StrCat({GetOverallMetricName(), ".", suffix});
}
}  // namespace password_manager
