// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELD_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELD_H_

#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Defines a field that is defined as required in an Autofill action.
struct RequiredField {
 public:
  enum class FieldValueStatus { kUnknown, kEmpty, kNotEmpty };

  ~RequiredField();
  RequiredField();
  RequiredField(const RequiredField& copy);

  void FromProto(const RequiredFieldProto& required_field_proto);

  // Returns true if fallback is required for this field.
  bool ShouldFallback(bool apply_fallback) const;

  // Returns whether this field has a value to fill the field with.
  bool HasValue() const;

  // The selector of the field that must be filled.
  Selector selector;

  RequiredFieldProto proto;

  // Defines whether the field is currently considered to be filled or not.
  FieldValueStatus status = FieldValueStatus::kUnknown;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELD_H_
