// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a copy of //components/autofill/core/browser/validation.h ~2023. It
// should be used only by //components/feedback/redaction_tool/.
// We need a copy because the //components/feedback/redaction_tool source code
// is shared into ChromeOS and needs to have no dependencies outside of //base/.
// TODO(b/281812289) Deduplicate the logic and let the autofill component
// depend on this one.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_VALIDATION_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_VALIDATION_H_

#include <string>

namespace redaction {

// Returns true if |text| looks like a valid credit card number.
// Uses the Luhn formula to validate the number.
bool IsValidCreditCardNumber(const std::string& text);

}  // namespace redaction

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_VALIDATION_H_
