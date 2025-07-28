// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"

#include <string_view>
#include <variant>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

AutofillType::ServerPrediction::ServerPrediction() = default;

AutofillType::ServerPrediction::ServerPrediction(const AutofillField& field) {
  password_requirements = field.password_requirements();
  server_predictions = field.server_predictions();
}

AutofillType::ServerPrediction::ServerPrediction(const ServerPrediction&) =
    default;

AutofillType::ServerPrediction& AutofillType::ServerPrediction::operator=(
    const ServerPrediction&) = default;

AutofillType::ServerPrediction::ServerPrediction(ServerPrediction&&) = default;

AutofillType::ServerPrediction& AutofillType::ServerPrediction::operator=(
    ServerPrediction&&) = default;

AutofillType::ServerPrediction::~ServerPrediction() = default;

FieldType AutofillType::ServerPrediction::server_type() const {
  return server_predictions.empty()
             ? NO_SERVER_DATA
             : ToSafeFieldType(server_predictions[0].type(), NO_SERVER_DATA);
}

bool AutofillType::ServerPrediction::is_override() const {
  return server_predictions.empty() ? false : server_predictions[0].override();
}

AutofillType::AutofillType(FieldType field_type)
    : type_(ToSafeFieldType(field_type, UNKNOWN_TYPE)) {}

AutofillType::AutofillType(HtmlFieldType field_type) : type_(field_type) {}

HtmlFieldType AutofillType::html_type() const {
  const HtmlFieldType* html_type = std::get_if<HtmlFieldType>(&type_);
  return html_type ? *html_type : HtmlFieldType::kUnspecified;
}

FieldTypeGroup AutofillType::group() const {
  return std::visit(absl::Overload{[](FieldType field_type) {
                                     return GroupTypeOfFieldType(field_type);
                                   },
                                   [](HtmlFieldType html_type) {
                                     return GroupTypeOfHtmlFieldType(html_type);
                                   }},
                    type_);
}

FieldType AutofillType::GetStorableType() const {
  return std::visit(
      absl::Overload{[](FieldType field_type) { return field_type; },
                     [](HtmlFieldType html_type) {
                       return HtmlFieldTypeToBestCorrespondingFieldType(
                           html_type);
                     }},
      type_);
}

std::string_view AutofillType::ToStringView() const {
  return std::visit(absl::Overload{[](FieldType field_type) {
                                     return FieldTypeToStringView(field_type);
                                   },
                                   [](HtmlFieldType html_type) {
                                     return FieldTypeToStringView(html_type);
                                   }},
                    type_);
}

}  // namespace autofill
