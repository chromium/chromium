// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/safety_config.h"

#include <cstddef>
#include <iterator>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

namespace {

using google::protobuf::RepeatedPtrField;

bool HasUnsafeScores(
    const RepeatedPtrField<proto::SafetyCategoryThreshold>& thresholds,
    const on_device_model::mojom::SafetyInfoPtr& safety_info) {
  CHECK(safety_info);
  CHECK(!safety_info->class_scores.empty());
  for (const auto& threshold : thresholds) {
    size_t output_index = static_cast<size_t>(threshold.output_index());
    if (static_cast<size_t>(output_index) >= safety_info->class_scores.size()) {
      // Needed to evaluate a score, but output was invalid. Mark it as unsafe.
      return true;
    }

    if (safety_info->class_scores.at(output_index) >= threshold.threshold()) {
      // Output score exceeded threshold.
      return true;
    }
  }

  // If it gets here, everything has passed.
  return false;
}

template <class T>
double GetLanguageReliabilityThreshold(const T& check, bool is_complete) {
  if (!check.has_language_check()) {
    return features::GetOnDeviceModelLanguageDetectionMinimumReliability();
  }
  if (is_complete || !check.language_check().has_partial_threshold()) {
    return check.language_check().confidence_threshold();
  }
  return check.language_check().partial_threshold();
}

using CheckTemplate =
    google::protobuf::RepeatedPtrField<proto::SubstitutedString>;

CheckTemplate MakeDefaultRawOutputCheckTemplate() {
  CheckTemplate input_template;
  auto* sub = input_template.Add();
  sub->set_string_template("%s");
  sub->add_substitutions()
      ->add_candidates()
      ->mutable_proto_field()
      ->add_proto_descriptors()
      ->set_tag_number(1);
  return input_template;
}

const CheckTemplate& GetRawOutputCheckTemplate(
    const proto::FeatureTextSafetyConfiguration& config) {
  if (!config.has_raw_output_check()) {
    const static base::NoDestructor<CheckTemplate> default_template(
        MakeDefaultRawOutputCheckTemplate());
    return *default_template;
  }
  return config.raw_output_check().input_template();
}

}  // namespace

SafetyConfig::SafetyConfig() = default;
SafetyConfig::SafetyConfig(
    std::optional<proto::FeatureTextSafetyConfiguration> proto)
    : proto_(proto) {}
SafetyConfig::SafetyConfig(SafetyConfig&&) = default;
SafetyConfig::~SafetyConfig() = default;
SafetyConfig& SafetyConfig::operator=(SafetyConfig&&) = default;

bool SafetyConfig::CanCheckPartialOutput(
    uint32_t num_output_tokens,
    uint32_t num_unchecked_output_tokens) const {
  if (!proto_) {
    // In the absence of a config, partial outputs are trivially checked at
    // every token.
    return true;
  }
  if (!proto_->has_partial_output_checks()) {
    return false;
  }
  if (num_output_tokens < proto_->partial_output_checks().minimum_tokens()) {
    return false;
  }
  if (num_unchecked_output_tokens <
      proto_->partial_output_checks().token_interval()) {
    return false;
  }
  return true;
}

bool SafetyConfig::IsTextInUnsupportedOrUndeterminedLanguage(
    const on_device_model::mojom::SafetyInfoPtr& safety_info,
    double reliability_threshold) const {
  auto& allowed_languages = proto_->allowed_languages();
  if (!proto_) {
    // No safety config, so no language requirements.
    return false;
  }

  if (allowed_languages.empty() || reliability_threshold <= 0.0) {
    // No language requirements.
    return false;
  }

  if (!safety_info->language) {
    // No language detection available, but language detection is required.
    // Treat as an unsupported language.
    return true;
  }

  if (!base::Contains(allowed_languages, safety_info->language->code)) {
    // Unsupported language.
    return true;
  }

  if (safety_info->language->reliability < reliability_threshold) {
    // Unreliable language detection. Treat as an unsupported language.
    return true;
  }

  // Language was detected reliably and is supported.
  return false;
}

int SafetyConfig::NumRequestChecks() const {
  return proto_ ? proto_->request_check_size() : 0;
}

std::optional<SubstitutionResult> SafetyConfig::GetRequestCheckInput(
    int check_idx,
    const google::protobuf::MessageLite& message) const {
  return CreateSubstitutions(message,
                             proto_->request_check(check_idx).input_template());
}

bool SafetyConfig::IsRequestCheckLanguageOnly(int check_idx) const {
  return proto_->request_check(check_idx).check_language_only();
}

bool SafetyConfig::IsRequestUnsafe(
    int check_idx,
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  const auto& check = proto_->request_check(check_idx);
  if (check.check_language_only()) {
    return false;
  }
  const auto& thresholds = check.safety_category_thresholds().empty()
                               ? proto_->safety_category_thresholds()
                               : check.safety_category_thresholds();
  return HasUnsafeScores(thresholds, safety_info);
}

bool SafetyConfig::IsRequestUnsupportedLanguage(
    int check_idx,
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  const auto& check = proto_->request_check(check_idx);
  if (check.ignore_language_result()) {
    return false;
  }
  double threshold =
      GetLanguageReliabilityThreshold(check, /*is_complete*/ true);
  return IsTextInUnsupportedOrUndeterminedLanguage(safety_info, threshold);
}

bool SafetyConfig::HasRawOutputCheck() const {
  return proto_.has_value() && NumResponseChecks() == 0;
}

std::optional<SubstitutionResult> SafetyConfig::GetRawOutputCheckInput(
    const std::string& raw_output) const {
  proto::StringValue message;
  message.set_value(raw_output);
  return CreateSubstitutions(message, GetRawOutputCheckTemplate(*proto_));
}

bool SafetyConfig::IsRawOutputUnsafe(
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  if (!proto_) {
    // If no safety config and we are allowed here, that means we don't care
    // about the safety scores so just mark the content as safe.
    return false;
  }
  return HasUnsafeScores(proto_->safety_category_thresholds(), safety_info);
}

bool SafetyConfig::IsRawOutputUnsupportedLanguage(
    bool is_complete,
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  const auto& check = proto_->raw_output_check();
  double threshold = GetLanguageReliabilityThreshold(check, is_complete);
  return IsTextInUnsupportedOrUndeterminedLanguage(safety_info, threshold);
}

int SafetyConfig::NumResponseChecks() const {
  return proto_ ? proto_->response_check_size() : 0;
}

bool SafetyConfig::IsResponseUnsafe(
    int check_idx,
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  const auto& check = proto_->response_check(check_idx);
  const auto& thresholds = check.safety_category_thresholds().empty()
                               ? proto_->safety_category_thresholds()
                               : check.safety_category_thresholds();
  return HasUnsafeScores(thresholds, safety_info);
}

bool SafetyConfig::IsResponseUnsupportedLanguage(
    int check_idx,
    bool is_complete,
    const on_device_model::mojom::SafetyInfoPtr& safety_info) const {
  const auto& check = proto_->response_check(check_idx);
  if (check.ignore_language_result()) {
    return false;
  }
  double threshold = GetLanguageReliabilityThreshold(check, is_complete);
  return IsTextInUnsupportedOrUndeterminedLanguage(safety_info, threshold);
}

std::optional<SubstitutionResult> SafetyConfig::GetResponseCheckInput(
    int check_idx,
    const google::protobuf::MessageLite& request,
    const google::protobuf::MessageLite& response) const {
  SubstitutionResult result;
  result.input = on_device_model::mojom::Input::New();
  for (const auto& input : proto_->response_check(check_idx).inputs()) {
    std::optional<SubstitutionResult> inner_result;
    switch (input.input_type()) {
      case proto::CHECK_INPUT_TYPE_REQUEST:
        inner_result = CreateSubstitutions(request, input.templates());
        break;
      case proto::CHECK_INPUT_TYPE_RESPONSE:
        inner_result = CreateSubstitutions(response, input.templates());
        break;
      default:
        return std::nullopt;
    }
    if (!inner_result) {
      return std::nullopt;
    }
    std::move(inner_result->input->pieces.begin(),
              inner_result->input->pieces.end(),
              std::back_inserter(result.input->pieces));
  }
  return result;
}

}  // namespace optimization_guide
