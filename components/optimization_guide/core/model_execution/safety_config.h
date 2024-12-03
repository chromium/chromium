// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CONFIG_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CONFIG_H_

#include <optional>
#include <string>

#include "components/optimization_guide/core/model_execution/substitution.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

class SafetyConfig final {
 public:
  SafetyConfig();
  explicit SafetyConfig(std::optional<proto::FeatureTextSafetyConfiguration>);
  SafetyConfig(SafetyConfig&&);
  SafetyConfig& operator=(SafetyConfig&&);
  ~SafetyConfig();

  // Returns true if partial output is ready to be evaluated.
  bool CanCheckPartialOutput(uint32_t num_output_tokens,
                             uint32_t num_unchecked_output_tokens) const;

  // Whether the text is in a language not supported by the safety classifier,
  // or the language could not be detected despite the classifier requiring one
  // or more specific languages.
  bool IsTextInUnsupportedOrUndeterminedLanguage(
      const on_device_model::mojom::SafetyInfoPtr& safety_info) const;

  // Whether scores indicate the output text is unsafe.
  bool IsUnsafeText(
      const on_device_model::mojom::SafetyInfoPtr& safety_info) const;

  // The number of request safety checks to perform.
  int NumRequestChecks() const;

  // Constructs input for a request safety check.
  // `check_idx` must be < `NumResponseChecks()`.
  std::optional<SubstitutionResult> GetRequestCheckInput(
      int check_idx,
      const google::protobuf::MessageLite& request_metadata) const;

  // Whether this check is only for allowed languages.
  bool IsRequestCheckLanguageOnly(int check_idx) const;

  // Whether the language result for this check should be ignored.
  bool ShouldIgnoreLanguageResultForRequestCheck(int check_idx) const;

  // Evaluates scores for a request safety check.
  // `check_idx` must be < `NumResponseChecks()`.
  bool IsRequestUnsafe(
      int check_idx,
      const on_device_model::mojom::SafetyInfoPtr& safety_info) const;

  // Whether this config has a special raw output check.
  bool HasRawOutputCheck() const;

  // Get the input for the raw output check.
  std::optional<SubstitutionResult> GetRawOutputCheckInput(
      const std::string&) const;

  // The number of request safety checks to perform.
  int NumResponseChecks() const;

  std::optional<SubstitutionResult> GetResponseCheckInput(
      int check_idx,
      const google::protobuf::MessageLite& request,
      const google::protobuf::MessageLite& response) const;

  // Whether the language result for this check should be ignored.
  bool ShouldIgnoreLanguageResultForResponseCheck(int check_idx) const;

  // Evaluates scores for a response safety check.
  // `check_idx` must be < `NumResponseChecks()`.
  bool IsResponseUnsafe(
      int check_idx,
      const on_device_model::mojom::SafetyInfoPtr& safety_info) const;

 private:
  std::optional<proto::FeatureTextSafetyConfiguration> proto_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SAFETY_CONFIG_H_
