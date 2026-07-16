// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/common/one_time_token_switches.h"

namespace one_time_tokens::switches {

const char kOneTimeTokenFetchEmailEndpointUrl[] =
    "one-time-token-fetch-email-endpoint-url";

const char kDefaultOneTimeTokenFetchEmailEndpointUrl[] =
    "https://onetimetoken.pa.googleapis.com/v1/onetimetokens:fetchEmail";

}  // namespace one_time_tokens::switches
