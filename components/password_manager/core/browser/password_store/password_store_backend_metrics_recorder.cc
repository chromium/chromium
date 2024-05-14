// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_backend_metrics_recorder.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace password_manager {

namespace {
constexpr char kMetricPrefix[] = "PasswordManager.PasswordStore";

bool HasRunToCompletion(
    PasswordStoreBackendMetricsRecorder::SuccessStatus success_status) {
  switch (success_status) {
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kSuccess:
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kError:
      return true;
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kCancelledTimeout:
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::
        kCancelledPwdSyncStateChanged:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder() =
    default;

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder(
    BackendInfix backend_infix,
    MethodName method_name,
    PasswordStoreAndroidBackendType store_type)
    : backend_infix_(std::move(backend_infix)),
      method_name_(std::move(method_name)),
      store_type_(store_type) {
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
    std::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const {
  RecordSuccess(success_status);
  if (HasRunToCompletion(success_status)) {
    RecordLatency();
    RecordRequestStatus(StoreBackendRequestStatus::kCompleted);
  } else if (success_status == SuccessStatus::kCancelledTimeout) {
    RecordRequestStatus(StoreBackendRequestStatus::kTimeout);
  } else if (success_status == SuccessStatus::kCancelledPwdSyncStateChanged) {
    RecordRequestStatus(
        StoreBackendRequestStatus::kCancelledPwdSyncStateChanged);
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
  // Infixes for the overall and backend specific histogram.
  std::vector<std::string> possible_infixes = {"Backend", *backend_infix_};
  // Adding the infix for split stores.
  if (store_type_ != PasswordStoreAndroidBackendType::kNone) {
    possible_infixes.push_back(GetStoreInfix());
  }

  for (const auto& infix : possible_infixes) {
    base::UmaHistogramEnumeration(
        base::JoinString({base::StrCat({kMetricPrefix, infix}), *method_name_},
                         "."),
        request_status);
  }
}

void PasswordStoreBackendMetricsRecorder::RecordSuccess(
    SuccessStatus success_status) const {
  // Infixes for the overall and backend specific histogram.
  std::vector<std::string> possible_infixes = {"Backend", *backend_infix_};
  // Adding the infix for split stores.
  if (store_type_ != PasswordStoreAndroidBackendType::kNone) {
    possible_infixes.push_back(GetStoreInfix());

    base::UmaHistogramBoolean(
        base::JoinString(
            {base::StrCat({kMetricPrefix, GetStoreInfix()}), "Success"}, "."),
        success_status == SuccessStatus::kSuccess);
  }

  for (const auto& infix : possible_infixes) {
    base::UmaHistogramBoolean(
        base::JoinString(
            {base::StrCat({kMetricPrefix, infix}), *method_name_, "Success"},
            "."),
        success_status == SuccessStatus::kSuccess);
  }
}

void PasswordStoreBackendMetricsRecorder::RecordErrorCode(
    const AndroidBackendError& backend_error) const {
  base::UmaHistogramEnumeration(
      base::StrCat({kMetricPrefix, "AndroidBackend.ErrorCode"}),
      backend_error.type);
  base::UmaHistogramEnumeration(
      base::JoinString({base::StrCat({kMetricPrefix, *backend_infix_}),
                        *method_name_, "ErrorCode"},
                       "."),
      backend_error.type);

  if (store_type_ != PasswordStoreAndroidBackendType::kNone) {
    base::UmaHistogramEnumeration(
        base::JoinString(
            {base::StrCat({kMetricPrefix, GetStoreInfix()}), "ErrorCode"}, "."),
        backend_error.type);
    base::UmaHistogramEnumeration(
        base::JoinString({base::StrCat({kMetricPrefix, GetStoreInfix()}),
                          *method_name_, "ErrorCode"},
                         "."),
        backend_error.type);
  }

  if (backend_error.type == AndroidBackendErrorType::kExternalError) {
    DCHECK(backend_error.api_error_code.has_value());
    RecordApiErrorCode(backend_error.api_error_code.value());
    LOG(ERROR) << "Password Manager API call for " << method_name_
               << " failed with error code: "
               << backend_error.api_error_code.value();
  }
  if (backend_error.connection_result_code.has_value()) {
    RecordConnectionResultCode(backend_error.connection_result_code.value());
  }
}

void PasswordStoreBackendMetricsRecorder::RecordLatency() const {
  base::TimeDelta duration = GetElapsedTimeSinceCreation();

  // Infixes for the overall and backend specific histogram.
  std::vector<std::string> possible_infixes = {"Backend", *backend_infix_};
  // Adding the infix for split stores.
  if (store_type_ != PasswordStoreAndroidBackendType::kNone) {
    possible_infixes.push_back(GetStoreInfix());
  }

  for (const auto& infix : possible_infixes) {
    base::UmaHistogramMediumTimes(
        base::JoinString(
            {base::StrCat({kMetricPrefix, infix}), *method_name_, "Latency"},
            "."),
        duration);
  }
}

void PasswordStoreBackendMetricsRecorder::RecordApiErrorCode(
    int api_error_code) const {
  base::UmaHistogramSparse(
      base::StrCat({kMetricPrefix, "AndroidBackend.APIError"}), api_error_code);
  base::UmaHistogramSparse(
      base::JoinString({base::StrCat({kMetricPrefix, *backend_infix_}),
                        *method_name_, "APIError"},
                       "."),
      api_error_code);

  if (store_type_ != PasswordStoreAndroidBackendType::kNone) {
    base::UmaHistogramSparse(
        base::JoinString(
            {base::StrCat({kMetricPrefix, GetStoreInfix()}), "APIError"}, "."),
        api_error_code);
    base::UmaHistogramSparse(
        base::JoinString({base::StrCat({kMetricPrefix, GetStoreInfix()}),
                          *method_name_, "APIError"},
                         "."),
        api_error_code);
  }
}

void PasswordStoreBackendMetricsRecorder::RecordConnectionResultCode(
    int connection_result_code) const {
  base::UmaHistogramSparse(
      base::StrCat({kMetricPrefix, "AndroidBackend.ConnectionResultCode"}),
      connection_result_code);
  base::UmaHistogramSparse(
      base::JoinString({base::StrCat({kMetricPrefix, *backend_infix_}),
                        *method_name_, "ConnectionResultCode"},
                       "."),
      connection_result_code);
  if (store_type_ != PasswordStoreAndroidBackendType::kNone) {
    base::UmaHistogramSparse(
        base::JoinString({base::StrCat({kMetricPrefix, GetStoreInfix()}),
                          *method_name_, "ConnectionResultCode"},
                         "."),
        connection_result_code);
  }
}

std::string PasswordStoreBackendMetricsRecorder::GetStoreInfix() const {
  switch (store_type_) {
    case PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
        kAccount:
      return base::JoinString({*backend_infix_, "Account"}, ".");
    case PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
        kLocal:
      return base::JoinString({*backend_infix_, "Local"}, ".");
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

}  // namespace password_manager
