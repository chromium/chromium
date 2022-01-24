// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FIELD_FORMATTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FIELD_FORMATTER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
namespace field_formatter {

// A key to retrieve data for value expressions.
struct Key {
  // An explicit integer key, this can retrieve a data point usually
  // originating from an Autofill source.
  explicit Key(int key);

  // A key from Autofill Assistant specific additions for data originating from
  // an Autofill source.
  explicit Key(AutofillFormatProto::AutofillAssistantCustomField custom_field);

  // A key from an Autofill source.
  explicit Key(autofill::ServerFieldType autofill_field);

  // A key from client memory.
  explicit Key(std::string memory_key);

  ~Key();
  Key(const Key&);

  bool operator<(const Key& other) const;
  bool operator==(const Key& other) const;

 private:
  friend class FieldFormatterStringTest;

  absl::optional<int> int_key;
  absl::optional<std::string> string_key;
};

// Replaces all placeholder occurrences of the form ${key} in |input| with the
// corresponding value in |mappings|, where |key| is an arbitrary string that
// does not contain curly braces. If |strict| is true, this will fail if any of
// the found placeholders is not in |mappings|. Otherwise, placeholders other
// than those from |mappings| will be left unchanged.
absl::optional<std::string> FormatString(
    const std::string& input,
    const base::flat_map<std::string, std::string>& mappings,
    bool strict = true);

// Turns a |value_expression| into a string, replacing |key| chunks with
// corresponding values in |mappings|. This will fail if any of the keys are
// not in |mappings|. If |quote_meta| the replacement pieces will be quoted.
ClientStatus FormatExpression(const ValueExpression& value_expression,
                              const base::flat_map<Key, std::string>& mappings,
                              bool quote_meta,
                              std::string* out_value);

// Returns a human-readable string representation of |value_expression| for
// use in logging and error reporting.
std::string GetHumanReadableValueExpression(
    const ValueExpression& value_expression);

// Creates a lookup map for all non-empty autofill and custom
// AutofillFormatProto::AutofillAssistantCustomField field types in
// |autofill_data_model|.
// |locale| should be a locale string such as "en-US".
template <typename T>
base::flat_map<Key, std::string> CreateAutofillMappings(
    const T& autofill_data_model,
    const std::string& locale);

}  // namespace field_formatter

// Debug output operator for value expressions. The output is only useful in
// debug builds.
std::ostream& operator<<(std::ostream& out,
                         const ValueExpression& value_expression);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_FIELD_FORMATTER_H_
