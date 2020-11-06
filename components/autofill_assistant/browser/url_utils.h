// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_URL_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_URL_UTILS_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace autofill_assistant {
namespace url_utils {

// Check whether |url| is in |domain| or in a subdomain of |domain|.
bool IsInDomainOrSubDomain(const GURL& url, const GURL& domain);

// Same as above, but checks against a vector of domains instead. Returns true
// if |url| is in |allowed_domains| or a subdomain of |allowed_domains|.
// NOTE: Domains should be specified without leading spec, e.g., "example.com".
bool IsInDomainOrSubDomain(const GURL& url,
                           const std::vector<std::string>& allowed_domains);

}  // namespace url_utils
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_URL_UTILS_H_
