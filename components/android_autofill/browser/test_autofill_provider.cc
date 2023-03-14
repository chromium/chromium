// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/test_autofill_provider.h"

namespace autofill {

bool TestAutofillProvider::GetCachedIsAutofilled(
    const FormFieldData& field) const {
  return false;
}

}  // namespace autofill
