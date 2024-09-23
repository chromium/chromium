// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_type.h"

#include <string_view>
#include <vector>

#include "base/notreached.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"

namespace autofill {

AutofillType::ServerPrediction::ServerPrediction() = default;

AutofillType::ServerPrediction::ServerPrediction(const AutofillField& field) {
  may_use_prefilled_placeholder = field.may_use_prefilled_placeholder();
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
    : server_type_(ToSafeFieldType(field_type, UNKNOWN_TYPE)) {}

AutofillType::AutofillType(HtmlFieldType field_type) : html_type_(field_type) {}

FieldTypeGroup AutofillType::group() const {
  return server_type_ != UNKNOWN_TYPE ? GroupTypeOfFieldType(server_type_)
                                      : GroupTypeOfHtmlFieldType(html_type_);
}

bool AutofillType::IsUnknown() const {
  return server_type_ == UNKNOWN_TYPE &&
         (html_type_ == HtmlFieldType::kUnspecified ||
          html_type_ == HtmlFieldType::kUnrecognized);
}

FieldType AutofillType::GetStorableType() const {
  return server_type_ != UNKNOWN_TYPE
             ? server_type_
             : HtmlFieldTypeToBestCorrespondingFieldType(html_type_);
}

std::string_view AutofillType::ToStringView() const {
  return IsUnknown()                    ? "UNKNOWN_TYPE"
         : server_type_ != UNKNOWN_TYPE ? FieldTypeToStringView(server_type_)
                                        : FieldTypeToStringView(html_type_);
}

}  // namespace autofill
