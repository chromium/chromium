// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/insecure_form_util.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"

namespace security_interstitials {

bool IsInsecureFormAction(const GURL& action_url) {
  // blob: and filesystem: URLs never hit the network, and access is restricted
  // to same-origin contexts, so they are not blocked. Some forms use
  // javascript URLs to handle submissions in JS, those don't count as mixed
  // content either.
  if (action_url.SchemeIs(url::kJavaScriptScheme) ||
      action_url.SchemeIs(url::kBlobScheme) ||
      action_url.SchemeIs(url::kFileSystemScheme)) {
    return false;
  }
  return !network::IsUrlPotentiallyTrustworthy(action_url);
}

}  // namespace security_interstitials
