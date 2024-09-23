// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_COMMON_WORDS_COMMON_WORDS_UTIL_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_COMMON_WORDS_COMMON_WORDS_UTIL_H_

#include <cstdint>
#include <string_view>

#include "base/containers/span.h"

namespace url_formatter::common_words {

// Returns true if |word| is included in Chrome's common word list.
bool IsCommonWord(std::string_view word);

// Overwrite the dafsa used, only for testing.
void SetCommonWordDAFSAForTesting(base::span<const uint8_t> dafsa);

// Reset the dafsa used, only for testing.
void ResetCommonWordDAFSAForTesting();

}  // namespace url_formatter::common_words

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_COMMON_WORDS_COMMON_WORDS_UTIL_H_
