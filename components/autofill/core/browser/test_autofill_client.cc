// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_client.h"

namespace autofill {

AutofillDriverFactory& TestAutofillClient::GetAutofillDriverFactory() {
  return autofill_driver_factory_;
}

}  // namespace autofill
