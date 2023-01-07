// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_REWRITE_BROWSER_URL_REQUEST_REWRITE_RULES_VALIDATION_H_
#define COMPONENTS_URL_REWRITE_BROWSER_URL_REQUEST_REWRITE_RULES_VALIDATION_H_

#include "components/url_rewrite/common/url_request_rewrite_rules.h"

namespace url_rewrite {

// Returns true if |rules| have valid data in them, false otherwise.
bool ValidateRules(const mojom::UrlRequestRewriteRules* rules);

}  // namespace url_rewrite

#endif  // COMPONENTS_URL_REWRITE_BROWSER_URL_REQUEST_REWRITE_RULES_VALIDATION_H_
