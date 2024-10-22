// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ERROR_MESSAGE_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_ERROR_MESSAGE_UTIL_H_

#include <set>
#include <string>
#include <vector>

class GURL;

namespace payments {

// Returns a developer-facing error message that the given payment |methods| are
// not supported.
std::string GetNotSupportedErrorMessage(const std::set<std::string>& methods);

// Returns a developer-facing error message that the apps are skipped because
// they do not support full delegation.
std::string GetAppsSkippedForPartialDelegationErrorMessage(
    const std::vector<std::string>& skipped_apps);

// Returns a developer-facing error message for failure to download a payment
// manifest due to the given HTTP response code.
std::string GenerateHttpStatusCodeError(const GURL& url,
                                        int http_response_code);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ERROR_MESSAGE_UTIL_H_
