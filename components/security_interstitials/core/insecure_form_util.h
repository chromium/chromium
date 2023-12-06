// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_INSECURE_FORM_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_INSECURE_FORM_UTIL_H_

class GURL;

namespace security_interstitials {

// Returns true if submitting a form with the given action url is insecure.
// Matches the blink check for mixed content at
// blink::MixedContentChecker::IsMixedFormAction().
bool IsInsecureFormAction(const GURL& action_url);

}  // namespace security_interstitials

#endif
