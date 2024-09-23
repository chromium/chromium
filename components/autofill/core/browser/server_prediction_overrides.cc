// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/server_prediction_overrides.h"

#include <optional>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

namespace {

using FieldSuggestion = AutofillQueryResponse::FormSuggestion::FieldSuggestion;
using FieldPrediction = FieldSuggestion::FieldPrediction;
using SingleServerPredictionOverride =
    std::pair<ServerPredictionOverrideKey, FieldSuggestion>;

constexpr std::string_view kSeparatorLevel0 = "-";
constexpr std::string_view kSeparatorLevel1 = "_";

// Parses a single type prediction for a field. If unsuccessful, it returns
// `std::nullopt`.
std::optional<FieldPrediction> ParseSingleFieldTypePrediction(
    std::string_view specification) {
  int32_t type = 0;
  if (!base::StringToInt(specification, &type)) {
    return std::nullopt;
  }

  FieldPrediction result;
  result.set_type(type);
  result.set_override(true);
  result.set_source(FieldPrediction::SOURCE_MANUAL_OVERRIDE);
  return result;
}

// Parses one or multiple server predictions and returns the corresponding
// `FieldSuggestion`.
base::expected<FieldSuggestion, std::string> ParseFieldTypePredictions(
    uint32_t field_signature,
    base::span<const std::string> specifications) {
  FieldSuggestion result;
  result.mutable_predictions()->Reserve(specifications.size());
  for (std::string_view spec : specifications) {
    std::optional<FieldPrediction> prediction =
        ParseSingleFieldTypePrediction(spec);
    if (!prediction) {
      return base::unexpected(
          base::StrCat({"unable to parse field prediction: ", spec}));
    }
    *result.add_predictions() = *std::move(prediction);
  }

  result.set_field_signature(field_signature);
  return result;
}

// Parses a single override of the form
// formsignature_fieldsignature[_serverprediction1][_serverprediction2]...
base::expected<SingleServerPredictionOverride, std::string>
ParseSingleServerPredictionOverride(std::string_view specification) {
  std::vector<std::string> spec_split =
      base::SplitString(specification, kSeparatorLevel1, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);

  if (spec_split.size() < 2) {
    return base::unexpected(
        base::StrCat({"expected string of form "
                      "formsignature_fieldsignature[_serverprediction1]..., "
                      "but received: ",
                      specification}));
  }

  // Parse form and field signature.
  uint64_t form_signature = 0;
  if (!base::StringToUint64(spec_split[0], &form_signature)) {
    return base::unexpected(
        base::StrCat({"unable to parse form signature: ", spec_split[0]}));
  }
  uint32_t field_signature = 0;
  if (!base::StringToUint(spec_split[1], &field_signature)) {
    return base::unexpected(
        base::StrCat({"unable to parse field signature: ", spec_split[1]}));
  }

  base::expected<FieldSuggestion, std::string> parsed_suggestions =
      ParseFieldTypePredictions(
          field_signature, base::span(spec_split).last(spec_split.size() - 2));
  if (!parsed_suggestions.has_value()) {
    return base::unexpected(parsed_suggestions.error());
  }

  return std::make_pair(std::make_pair(FormSignature(form_signature),
                                       FieldSignature(field_signature)),
                        std::move(parsed_suggestions).value());
}

}  // namespace

base::expected<ServerPredictionOverrides, std::string>
ParseServerPredictionOverrides(std::string_view specification) {
  // Split the string into individual overrides.
  std::vector<std::string> overrides =
      base::SplitString(specification, kSeparatorLevel0, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_ALL);

  // Parse each override.
  ServerPredictionOverrides result;
  for (const auto& override : overrides) {
    base::expected<SingleServerPredictionOverride, std::string>
        parsed_override = ParseSingleServerPredictionOverride(override);
    if (!parsed_override.has_value()) {
      return base::unexpected(parsed_override.error());
    }
    const ServerPredictionOverrideKey& override_key =
        parsed_override.value().first;
    FieldSuggestion& suggestion = parsed_override.value().second;
    result[override_key].push_back(std::move(suggestion));
  }

  return result;
}

}  // namespace autofill
