// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

namespace payments::facilitated {

bool FacilitatedPaymentsClient::ShowPixPaymentPrompt() {
  return false;
}

}  // namespace payments::facilitated
