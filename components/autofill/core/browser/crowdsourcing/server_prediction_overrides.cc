// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/server_prediction_overrides.h"

#include <optional>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/autofill/core/browser/field_types.h"
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

base::expected<FormSignature, std::string> ParseFormSignature(
    std::string_view s) {
  uint64_t form_signature = 0;
  if (!base::StringToUint64(s, &form_signature)) {
    return base::unexpected(
        base::StrCat({"unable to parse form signature: ", s}));
  }
  return FormSignature(form_signature);
}

base::expected<FieldSignature, std::string> ParseFieldSignature(
    std::string_view s) {
  uint32_t field_signature = 0;
  if (!base::StringToUint(s, &field_signature)) {
    return base::unexpected(
        base::StrCat({"unable to parse field signature: ", s}));
  }
  return FieldSignature(field_signature);
}

FieldPrediction CreatePrediction(int32_t field_type) {
  FieldPrediction result;
  result.set_type(field_type);
  result.set_override(true);
  result.set_source(FieldPrediction::SOURCE_MANUAL_OVERRIDE);
  return result;
}

// Parses a single type prediction for a field. If unsuccessful, it returns
// `std::nullopt`.
std::optional<FieldPrediction> ParseSingleFieldTypePrediction(
    std::string_view specification) {
  int32_t type = 0;
  if (!base::StringToInt(specification, &type)) {
    return std::nullopt;
  }
  return CreatePrediction(type);
}

// Parses one or multiple server predictions and returns the corresponding
// `FieldSuggestion`.
base::expected<FieldSuggestion, std::string> ParseFieldTypePredictions(
    FieldSignature field_signature,
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

  result.set_field_signature(*field_signature);
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
  base::expected<FormSignature, std::string> form_signature =
      ParseFormSignature(spec_split[0]);
  if (!form_signature.has_value()) {
    return base::unexpected(form_signature.error());
  }
  base::expected<FieldSignature, std::string> field_signature =
      ParseFieldSignature(spec_split[1]);
  if (!field_signature.has_value()) {
    return base::unexpected(field_signature.error());
  }
  base::expected<FieldSuggestion, std::string> parsed_suggestions =
      ParseFieldTypePredictions(
          field_signature.value(),
          base::span(spec_split).last(spec_split.size() - 2));
  if (!parsed_suggestions.has_value()) {
    return base::unexpected(parsed_suggestions.error());
  }

  return std::make_pair(
      std::make_pair(form_signature.value(), field_signature.value()),
      std::move(parsed_suggestions).value());
}

// Converts a JSON object into predictions. See the inline comments for the
// expected JSON format.
base::expected<ServerPredictionOverrides, std::string>
ParseServerPredictionOverrideJson(const base::Value& value) {
  ServerPredictionOverrides result;

  // { "<form_signature>": <form_value> }
  if (!value.is_dict()) {
    return base::unexpected("JSON value must be dict");
  }
  for (const auto [form_signature_string, form_value] : value.GetDict()) {
    base::expected<FormSignature, std::string> form_signature =
        ParseFormSignature(form_signature_string);
    if (!form_signature.has_value()) {
      return base::unexpected(form_signature.error());
    }

    // { "<form_signature>": { "<field_signature>": <field_value> } }
    if (!form_value.is_dict()) {
      return base::unexpected("form_value must be a dict");
    }
    for (const auto [field_signature_string, field_value] :
         form_value.GetDict()) {
      base::expected<FieldSignature, std::string> field_signature =
          ParseFieldSignature(field_signature_string);
      if (!field_signature.has_value()) {
        return base::unexpected(field_signature.error());
      }
      if (!field_value.is_list()) {
        return base::unexpected("field_value must be a list");
      }

      // {
      //   "<form_signature>": {
      //     "<field_signature>": [
      //       <suggestion_value>,
      //       ...
      //     ]
      //   }
      // }
      for (const base::Value& suggestion_value : field_value.GetList()) {
        if (!suggestion_value.is_dict()) {
          return base::unexpected("suggestion_value must be a dict");
        }
        const base::Value::Dict& suggestion_dict = suggestion_value.GetDict();
        FieldSuggestion& suggestion =
            result[{form_signature.value(), field_signature.value()}]
                .emplace_back();

        // {
        //   "<form_signature>": {
        //     "<field_signature>": [
        //       {
        //         "predictions": [ <raw_field_type> or "<type_name>", ... ],
        //         "format_string": <format_string>
        //       },
        //       ...
        //     ]
        //   }
        // }
        if (const base::Value::List* predictions =
                suggestion_dict.FindList("predictions")) {
          for (const base::Value& prediction : *predictions) {
            if (std::optional<int> raw_field_type = prediction.GetIfInt()) {
              *suggestion.add_predictions() = CreatePrediction(*raw_field_type);
            } else if (const std::string* type_name =
                           prediction.GetIfString()) {
              FieldType field_type = TypeNameToFieldType(*type_name);
              *suggestion.add_predictions() = CreatePrediction(field_type);
            }
          }
        }
        if (const std::string* format_string_type =
                suggestion_dict.FindString("format_string_type")) {
          FormatString_Type type;
          if (FormatString_Type_Parse(*format_string_type, &type)) {
            suggestion.mutable_format_string()->set_type(type);
          }
        }
        if (const std::string* format_string =
                suggestion_dict.FindString("format_string")) {
          suggestion.mutable_format_string()->set_format_string(*format_string);
        }
      }
    }
  }
  return result;
}

}  // namespace

base::expected<ServerPredictionOverrides, std::string>
ParseServerPredictionOverrides(std::string_view overrides_as_string,
                               OverrideFormat format) {
  switch (format) {
    case OverrideFormat::kSpec: {
      // Split the string into individual overrides.
      std::vector<std::string> overrides =
          base::SplitString(overrides_as_string, kSeparatorLevel0,
                            base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
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
    case OverrideFormat::kJson: {
      std::string json;
      if (!base::Base64Decode(overrides_as_string, &json,
                              base::Base64DecodePolicy::kForgiving)) {
        return base::unexpected("Base64Decode() failed");
      }
      base::expected<base::Value, base::JSONReader::Error> overrides =
          base::JSONReader::ReadAndReturnValueWithError(json);
      if (!overrides.has_value()) {
        return base::unexpected(overrides.error().ToString());
      }
      return ParseServerPredictionOverrideJson(std::move(overrides).value());
    }
  }
  NOTREACHED();
}

}  // namespace autofill
