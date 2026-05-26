// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_ERROR_CONVERTER_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_ERROR_CONVERTER_H_

#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_error_details.pb.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace one_time_tokens {

// Converts an Email One Time Token Fetch backend error reason code to a
// client-side OneTimeTokenRetrievalError.
OneTimeTokenRetrievalError ConvertEmailOneTimeTokenFetchErrorReason(
    google::internal::chrome::passwords::onetimetoken::v1::
        FetchEmailOneTimeTokenErrorDetails::ReasonCode reason_code);

// Converts a `FetchEmailOneTimeTokenErrorDetails` proto containing potentially
// multiple server-side reason codes into a single client-side
// `OneTimeTokenRetrievalError`.
OneTimeTokenRetrievalError ConvertEmailOneTimeTokenFetchErrorDetails(
    const google::internal::chrome::passwords::onetimetoken::v1::
        FetchEmailOneTimeTokenErrorDetails& error_details);

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_EMAIL_ONE_TIME_TOKEN_FETCH_ERROR_CONVERTER_H_
