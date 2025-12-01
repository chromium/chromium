// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_RETRIEVAL_ERROR_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_RETRIEVAL_ERROR_H_

namespace one_time_tokens {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OneTimeTokenRetrievalError {
  kUnknown = 0,
  kMaxValue = kUnknown,
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_RETRIEVAL_ERROR_H_
