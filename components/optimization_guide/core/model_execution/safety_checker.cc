// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/safety_checker.h"

#include "base/barrier_callback.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"

namespace optimization_guide {

namespace {

proto::InternalOnDeviceModelExecutionInfo MakeTextSafetyExecutionLog(
    const std::string& text,
    const on_device_model::mojom::SafetyInfoPtr& safety_info,
    bool is_unsafe) {
  proto::InternalOnDeviceModelExecutionInfo ts_execution_info;
  ts_execution_info.mutable_request()
      ->mutable_text_safety_model_request()
      ->set_text(text);
  auto* ts_resp = ts_execution_info.mutable_response()
                      ->mutable_text_safety_model_response();
  *ts_resp->mutable_scores() = {safety_info->class_scores.begin(),
                                safety_info->class_scores.end()};
  ts_resp->set_is_unsafe(is_unsafe);
  if (safety_info->language) {
    ts_resp->set_language_code(safety_info->language->code);
    ts_resp->set_language_confidence(safety_info->language->reliability);
  }
  return ts_execution_info;
}

on_device_model::mojom::SafetyInfoPtr AsSafetyInfo(
    on_device_model::mojom::LanguageDetectionResultPtr result) {
  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->language = std::move(result);
  return safety_info;
}

SafetyChecker::Result FailToRunResult() {
  SafetyChecker::Result result;
  result.failed_to_run = true;
  return result;
}

SafetyChecker::Result RequestCheckResult(
    base::WeakPtr<SafetyChecker> checker,
    int request_check_idx,
    std::string check_input_text,
    on_device_model::mojom::SafetyInfoPtr safety_info) {
  if (!checker) {
    return FailToRunResult();
  }
  SafetyChecker::Result result;
  // Evaluate the check.
  result.is_unsafe =
      checker->safety_cfg().IsRequestUnsafe(request_check_idx, safety_info);
  result.is_unsupported_language =
      !checker->safety_cfg().ShouldIgnoreLanguageResultForRequestCheck(
          request_check_idx) &&
      checker->safety_cfg().IsTextInUnsupportedOrUndeterminedLanguage(
          safety_info);
  *result.logs.Add() = MakeTextSafetyExecutionLog(check_input_text, safety_info,
                                                  result.is_unsafe);
  return result;
}

SafetyChecker::Result RawOutputCheckResult(
    base::WeakPtr<SafetyChecker> checker,
    std::string check_input_text,
    on_device_model::mojom::SafetyInfoPtr safety_info) {
  if (!checker) {
    return FailToRunResult();
  }
  SafetyChecker::Result result;
  // Evaluate the check.
  result.is_unsafe = checker->safety_cfg().IsUnsafeText(safety_info);
  result.is_unsupported_language =
      checker->safety_cfg().IsTextInUnsupportedOrUndeterminedLanguage(
          safety_info);
  *result.logs.Add() = MakeTextSafetyExecutionLog(check_input_text, safety_info,
                                                  result.is_unsafe);
  return result;
}

SafetyChecker::Result ResponseCheckResult(
    base::WeakPtr<SafetyChecker> checker,
    int request_check_idx,
    std::string check_input_text,
    on_device_model::mojom::SafetyInfoPtr safety_info) {
  if (!checker) {
    return FailToRunResult();
  }
  SafetyChecker::Result result;
  // Evaluate the check.
  result.is_unsafe =
      checker->safety_cfg().IsResponseUnsafe(request_check_idx, safety_info);
  result.is_unsupported_language =
      !checker->safety_cfg().ShouldIgnoreLanguageResultForResponseCheck(
          request_check_idx) &&
      checker->safety_cfg().IsTextInUnsupportedOrUndeterminedLanguage(
          safety_info);
  *result.logs.Add() = MakeTextSafetyExecutionLog(check_input_text, safety_info,
                                                  result.is_unsafe);
  return result;
}

}  // namespace

TextSafetyClient::~TextSafetyClient() = default;

SafetyChecker::Result::Result() = default;
SafetyChecker::Result::Result(const Result&) = default;
SafetyChecker::Result::Result(Result&&) = default;
SafetyChecker::Result& SafetyChecker::Result::operator=(Result&&) = default;
SafetyChecker::Result::~Result() = default;

// static
SafetyChecker::Result SafetyChecker::Result::Merge(
    std::vector<Result> results) {
  Result merged;
  for (Result& result : results) {
    merged.failed_to_run |= result.failed_to_run;
    merged.is_unsafe |= result.is_unsafe;
    merged.is_unsupported_language |= result.is_unsupported_language;
    merged.logs.MergeFrom(std::move(result.logs));
  }
  return merged;
}

SafetyChecker::SafetyChecker(SafetyConfig safety_cfg)
    : safety_cfg_(std::move(safety_cfg)) {}
SafetyChecker::~SafetyChecker() = default;

void SafetyChecker::RunRequestChecks(
    TextSafetyClient& client,
    const google::protobuf::MessageLite& request,
    ResultCallback callback) {
  int num_checks = safety_cfg_.NumRequestChecks();
  if (num_checks == 0) {
    std::move(callback).Run(SafetyChecker::Result{});
    return;
  }
  auto merge_fn = base::BarrierCallback<Result>(
      num_checks,
      base::BindOnce(&SafetyChecker::Result::Merge).Then(std::move(callback)));
  for (int idx = 0; idx < num_checks; idx++) {
    auto check_input = safety_cfg_.GetRequestCheckInput(idx, request);
    if (!check_input) {
      merge_fn.Run(FailToRunResult());
      continue;
    }
    auto text = check_input->ToString();
    auto merge_result_fn =
        base::BindOnce(&RequestCheckResult, weak_ptr_factory_.GetWeakPtr(), idx,
                       text)
            .Then(merge_fn);
    if (safety_cfg_.IsRequestCheckLanguageOnly(idx)) {
      client.GetTextSafetyModelRemote()->DetectLanguage(
          text, base::BindOnce(&AsSafetyInfo).Then(std::move(merge_result_fn)));
    } else {
      client.GetTextSafetyModelRemote()->ClassifyTextSafety(
          text, std::move(merge_result_fn));
    }
  }
}

void SafetyChecker::RunRawOutputCheck(TextSafetyClient& client,
                                      const std::string& raw_output,
                                      ResultCallback callback) {
  if (!safety_cfg_.HasRawOutputCheck()) {
    std::move(callback).Run(SafetyChecker::Result{});
    return;
  }
  auto check_input = safety_cfg_.GetRawOutputCheckInput(raw_output);
  if (!check_input) {
    std::move(callback).Run(FailToRunResult());
    return;
  }
  auto text = check_input->ToString();
  client.GetTextSafetyModelRemote()->ClassifyTextSafety(
      text, base::BindOnce(&RawOutputCheckResult,
                           weak_ptr_factory_.GetWeakPtr(), text)
                .Then(std::move(callback)));
}

void SafetyChecker::RunResponseChecks(
    TextSafetyClient& client,
    const google::protobuf::MessageLite& request,
    const proto::Any& response_as_any,
    ResultCallback callback) {
  int num_checks = safety_cfg_.NumResponseChecks();
  if (num_checks == 0) {
    std::move(callback).Run(SafetyChecker::Result{});
    return;
  }
  auto response = GetProtoFromAny(response_as_any);
  if (!response) {
    std::move(callback).Run(FailToRunResult());
    return;
  }
  auto merge_fn = base::BarrierCallback<Result>(
      num_checks,
      base::BindOnce(&SafetyChecker::Result::Merge).Then(std::move(callback)));
  for (int idx = 0; idx < num_checks; idx++) {
    auto check_input =
        safety_cfg_.GetResponseCheckInput(idx, request, *response);
    if (!check_input) {
      merge_fn.Run(FailToRunResult());
      continue;
    }
    auto text = check_input->ToString();
    auto merge_result_fn =
        base::BindOnce(&ResponseCheckResult, weak_ptr_factory_.GetWeakPtr(),
                       idx, text)
            .Then(merge_fn);
    client.GetTextSafetyModelRemote()->ClassifyTextSafety(
        text, std::move(merge_result_fn));
  }
}

}  // namespace optimization_guide
