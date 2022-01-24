// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_VALUE_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_VALUE_UTIL_H_

#include <ostream>
#include <string>
#include <vector>
#include "components/autofill_assistant/browser/model.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Custom comparison operators for |ValueProto|, because we can't use
// |MessageDifferencer| for protobuf lite and can't rely on serialization.
bool operator==(const ValueProto& value_a, const ValueProto& value_b);
bool operator!=(const ValueProto& value_a, const ValueProto& value_b);
bool operator<(const ValueProto& value_a, const ValueProto& value_b);
bool operator>(const ValueProto& value_a, const ValueProto& value_b);

// Custom comparison operator for |ModelValue|.
bool operator==(const ModelProto::ModelValue& value_a,
                const ModelProto::ModelValue& value_b);

// Custom comparison operator for |ChipProto|.
bool operator==(const ChipProto& value_a, const ChipProto& value_b);

// Custom comparison operator for |DirectActionProto|.
bool operator==(const DirectActionProto& value_a,
                const DirectActionProto& value_b);

// Custom comparison operator for |UserActionProto|.
bool operator==(const UserActionProto& value_a, const UserActionProto& value_b);

// Custom comparison operators for |DateProto|.
bool operator==(const DateProto& value_a, const DateProto& value_b);
bool operator<(const DateProto& value_a, const DateProto& value_b);

// Custom comparison operator for |AutofillCreditCardProto|.
bool operator==(const AutofillCreditCardProto& value_a,
                const AutofillCreditCardProto& value_b);

// Custom comparison operator for |AutofillProfileProto|.
bool operator==(const AutofillProfileProto& value_a,
                const AutofillProfileProto& value_b);

// Custom comparison operator for |LoginOptionProto|.
bool operator==(const LoginOptionProto& value_a,
                const LoginOptionProto& value_b);

// Custom comparison operator for |CreditCardResponseProto|.
bool operator==(const CreditCardResponseProto& value_a,
                const CreditCardResponseProto& value_b);

// Intended for debugging.
std::ostream& operator<<(std::ostream& out, const ValueProto& value);
std::ostream& operator<<(std::ostream& out,
                         const ValueReferenceProto& reference);
std::ostream& operator<<(std::ostream& out,
                         const ModelProto::ModelValue& value);
std::ostream& operator<<(std::ostream& out, const UserActionProto& value);
std::ostream& operator<<(std::ostream& out, const DateProto& value);
std::ostream& operator<<(std::ostream& out,
                         const AutofillCreditCardProto& value);
std::ostream& operator<<(std::ostream& out, const AutofillProfileProto& value);
std::ostream& operator<<(std::ostream& out, const LoginOptionProto& value);
std::ostream& operator<<(std::ostream& out,
                         const CreditCardResponseProto& value);

// Convenience constructors.
ValueProto SimpleValue(bool value, bool is_client_side_only = true);
ValueProto SimpleValue(const std::string& value,
                       bool is_client_side_only = true);
ValueProto SimpleValue(int value, bool is_client_side_only = true);
ValueProto SimpleValue(const DateProto& value, bool is_client_side_only = true);
ModelProto::ModelValue SimpleModelValue(const std::string& identifier,
                                        const ValueProto& value);

// Returns true if all |values| share the specified |target_type|.
bool AreAllValuesOfType(const std::vector<ValueProto>& values,
                        ValueProto::KindCase target_type);

// Returns true if all |values| share the specified |target_size|.
bool AreAllValuesOfSize(const std::vector<ValueProto>& values, int target_size);

// Returns if any of the provided |values| has |is_client_side_only| = true.
bool ContainsClientOnlyValue(const std::vector<ValueProto>& values);

// Returns the number of elements in |value|.
int GetValueSize(const ValueProto& value);

// Returns the |index|'th item of |value| or nullopt if |index| is
// out-of-bounds.
absl::optional<ValueProto> GetNthValue(const ValueProto& value, int index);

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_VALUE_UTIL_H_
