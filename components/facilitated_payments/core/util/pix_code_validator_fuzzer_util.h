// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PIX_CODE_VALIDATOR_FUZZER_UTIL_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PIX_CODE_VALIDATOR_FUZZER_UTIL_H_

namespace payments::facilitated {

// The regular expression pattern for a valid PIX code, as used by the fuzzer
// tests to generate more test cases.
extern const char kPixCodeValidatorFuzzerDomainRegexPattern[];

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PIX_CODE_VALIDATOR_FUZZER_UTIL_H_
