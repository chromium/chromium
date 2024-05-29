// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_INSECURE_FORM_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_INSECURE_FORM_UTIL_H_

#include "build/build_config.h"

class GURL;

namespace security_interstitials {

#if BUILDFLAG(IS_IOS)
// On iOS, EarlGrey tests can't serve a valid HTTPS response due to platform
// restrictions, so it's not possible to reliably use https:// URLs in Insecure
// Form Warning tests. Also, the test server serves URLs over 127.0.01 which is
// a trustworthy URL. This prevents serving a form over HTTPS and having it post
// to an insecure URL on iOS.
//
// To work around this, this function sets two ports for tests:
// - A form whose source URL's port is `source_url_port_treated_as_secure` will
//   be treated as an HTTPS URL in IsInsecureFormSourceAndAction().
// - A form whose action URL's port is `action_url_port_treated_as_insecure`
//   will be treated as an insecure URL in IsInsecureFormSourceAndAction().
//
// This allows us to use the test server on iOS and have the form's source and
// action URLs as 127.0.0.1:<port>.
void SetInsecureFormPortsForTesting(int source_url_port_treated_as_secure,
                                    int action_url_port_treated_as_insecure);
#endif

// Returns true if submitting a form with the given source and action urls is
// insecure.
// `source_url` is the URL of the page that submits the form.
// `action_url` is the URL of the form's action attribute.
bool IsInsecureFormActionOnSecureSource(const GURL& source_url,
                                        const GURL& action_url);

// Returns true if submitting a form with the given action url is insecure.
// Matches the blink check for mixed content at
// blink::MixedContentChecker::IsMixedFormAction().
// `action_url` is the URL of the form's action attribute.
bool IsInsecureFormAction(const GURL& action_url);

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_INSECURE_FORM_UTIL_H_
