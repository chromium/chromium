// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PIX_CODE_VALIDATOR_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PIX_CODE_VALIDATOR_H_

#include <string>

namespace payments::facilitated {

// Returns true if the input `code` is a valid PIX code, i.e.:
// 1) It consists of valid PIX code sections.
// 2) The first section is the payload format indicator.
// 3) The last section is the CRC16.
// 4) The merchant account information section contains valid subsections,
//    including the PIX code indicator as the first subsection.
// 5) The additional data field template section, if present, contains valid
//    subsections.
//
// This function does not validate the value of the CRC16.
bool IsValidPixCode(std::string_view code);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_UTIL_PIX_CODE_VALIDATOR_H_
