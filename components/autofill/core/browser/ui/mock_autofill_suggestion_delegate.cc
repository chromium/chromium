// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"

namespace autofill {

MockAutofillSuggestionDelegate::MockAutofillSuggestionDelegate() = default;

MockAutofillSuggestionDelegate::~MockAutofillSuggestionDelegate() = default;

base::WeakPtr<MockAutofillSuggestionDelegate>
MockAutofillSuggestionDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
