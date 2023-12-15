// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/features/features.h"

namespace payments::facilitated {

// When enabled, Chrome will detect PIX codes on allow-listed merchant websites.
BASE_FEATURE(kEnablePixDetection,
             "EnablePixDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will offer to pay with accounts supporting Pix.
BASE_FEATURE(kEnablePixPayments,
             "EnablePixPayments",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace payments::facilitated
