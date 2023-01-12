// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/mock_autofill_popup_delegate.h"

namespace autofill {

MockAutofillPopupDelegate::MockAutofillPopupDelegate() = default;

MockAutofillPopupDelegate::~MockAutofillPopupDelegate() = default;

base::WeakPtr<MockAutofillPopupDelegate>
MockAutofillPopupDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
