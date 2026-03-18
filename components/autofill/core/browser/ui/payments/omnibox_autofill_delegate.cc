// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/omnibox_autofill_delegate.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

OmniboxAutofillDelegate::OmniboxAutofillDelegate(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

OmniboxAutofillDelegate::~OmniboxAutofillDelegate() = default;

}  // namespace autofill
