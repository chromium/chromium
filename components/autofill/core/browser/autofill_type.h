// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TYPE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"

namespace autofill {

class AutofillField;

// The high-level description of Autofill types, used to categorize form fields
// and for associating form fields with form values in the Web Database.
class AutofillType {
 public:
  // A collection of server prediction metadata related to a form field.
  // Its current intended use is solely for consumers outside of
  // components/autofill.
  // TODO(crbug.com/40232021): Move all server prediction related information
  // from `AutofillField` here, add it as a member to `AutofillType` and use it
  // inside `AutofillField`.
  struct ServerPrediction {
    ServerPrediction();
    explicit ServerPrediction(const AutofillField& field);

    ServerPrediction(const ServerPrediction&);
    ServerPrediction& operator=(const ServerPrediction&);
    ServerPrediction(ServerPrediction&&);
    ServerPrediction& operator=(ServerPrediction&&);

    ~ServerPrediction();

    // The most likely server-side prediction for the field's type.
    FieldType server_type() const;

    // Checks whether server-side prediction for the field's type is an
    // override.
    bool is_override() const;

    // Whether the server-side classification indicates that the field
    // may be pre-filled with a placeholder in the value attribute.
    // For autofillable types, `nullopt` indicates that there is no server-side
    // classification. For PWM, `nullopt` and `false` are currently identical.
    std::optional<bool> may_use_prefilled_placeholder = std::nullopt;

    // Requirements the site imposes on passwords (for password generation)
    // obtained from the Autofill server.
    std::optional<PasswordRequirementsSpec> password_requirements;

    // The server-side predictions for the field's type.
    std::vector<
        AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>
        server_predictions;
  };

  explicit AutofillType(FieldType field_type = NO_SERVER_DATA);
  explicit AutofillType(HtmlFieldType field_type);
  AutofillType(const AutofillType& autofill_type) = default;
  AutofillType& operator=(const AutofillType& autofill_type) = default;

  HtmlFieldType html_type() const { return html_type_; }

  FieldTypeGroup group() const;

  // Returns true if both the `server_type_` and the `html_type_` are set to
  // their respective enum's unknown value.
  bool IsUnknown() const;

  // Maps `this` type to a field type that can be directly stored in an Autofill
  // data model (in the sense that it makes sense to call
  // `AutofillDataModel::SetRawInfo()` with the returned field type as the first
  // parameter).  Note that the returned type might not be exactly equivalent to
  // `this` type.  For example, the HTML types 'country' and 'country-name' both
  // map to ADDRESS_HOME_COUNTRY.
  FieldType GetStorableType() const;

  std::string_view ToStringView() const;

 private:
  // The server-native field type, or UNKNOWN_TYPE if unset.
  FieldType server_type_ = UNKNOWN_TYPE;

  // The HTML autocomplete field type, if set.
  HtmlFieldType html_type_ = HtmlFieldType::kUnspecified;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TYPE_H_
