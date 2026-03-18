// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_

#include "base/memory/raw_ref.h"

namespace autofill {

class AutofillClient;

class OmniboxAutofillDelegate {
 public:
  explicit OmniboxAutofillDelegate(AutofillClient* autofill_client);

  OmniboxAutofillDelegate(const OmniboxAutofillDelegate&) = delete;
  OmniboxAutofillDelegate& operator=(const OmniboxAutofillDelegate&) = delete;

  ~OmniboxAutofillDelegate();

 private:
  const raw_ref<AutofillClient> client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_OMNIBOX_AUTOFILL_DELEGATE_H_
