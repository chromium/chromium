// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_CONSTANTS_H_

#include "base/time/time.h"

namespace autofill::payments {

// The client-side timeout allowed for a card upload (or create) request.
inline constexpr base::TimeDelta kUploadCardRequestTimeout =
    base::Milliseconds(6500);

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_PAYMENTS_REQUEST_CONSTANTS_H_
