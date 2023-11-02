// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_COMMON_WORDS_COMMON_WORDS_UTIL_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_COMMON_WORDS_COMMON_WORDS_UTIL_H_

#include "base/strings/string_piece.h"

namespace url_formatter {

namespace common_words {

// Returns true if |word| is included in Chrome's common word list.
bool IsCommonWord(base::StringPiece word);

// Overwrite the dafsa used, only for testing.
void SetCommonWordDAFSAForTesting(const unsigned char* dafsa, size_t length);

// Reset the dafsa used, only for testing.
void ResetCommonWordDAFSAForTesting();

}  // namespace common_words

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_COMMON_WORDS_COMMON_WORDS_UTIL_H_
