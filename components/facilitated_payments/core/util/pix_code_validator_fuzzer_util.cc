// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/pix_code_validator_fuzzer_util.h"

namespace payments::facilitated {

const char kPixCodeValidatorFuzzerDomainRegexPattern[] = "^([0-9]{4}.+)+$";

}  // namespace payments::facilitated
