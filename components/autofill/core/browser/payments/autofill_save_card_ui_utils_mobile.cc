// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_ui_utils_mobile.h"

#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"

namespace autofill {

int GetSaveCardIconId(bool is_google_pay_branding_enabled) {
  return is_google_pay_branding_enabled ? IDR_AUTOFILL_GOOGLE_PAY
                                        : IDR_INFOBAR_AUTOFILL_CC;
}

}  // namespace autofill
