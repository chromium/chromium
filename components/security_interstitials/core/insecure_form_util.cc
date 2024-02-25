// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/insecure_form_util.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS)

namespace {

// If a <form>'s source URL (the URL that the form is hosted on) has this port,
// it'll be treated as an HTTPS URL in the Insecure Form warning checks.
static int g_form_source_url_port_treated_as_secure_for_insecure_form_testing =
    0;

// If a <form>'s action URL (the URL that the form posts to) has this port,
// it'll be treated as an insecure URL in the Insecure Form warning checks.
static int
    g_form_action_url_port_treated_as_insecure_for_insecure_form_testing = 0;

}  // namespace

#endif

namespace security_interstitials {

#if BUILDFLAG(IS_IOS)

void SetInsecureFormPortsForTesting(int source_url_port_treated_as_secure,
                                    int action_url_port_treated_as_insecure) {
  g_form_source_url_port_treated_as_secure_for_insecure_form_testing =
      source_url_port_treated_as_secure;
  g_form_action_url_port_treated_as_insecure_for_insecure_form_testing =
      action_url_port_treated_as_insecure;
}

#endif

bool IsInsecureFormActionOnSecureSource(const GURL& source_url,
                                        const GURL& action_url) {
  if (!source_url.SchemeIs(url::kHttpsScheme)) {
#if BUILDFLAG(IS_IOS)
    // On iOS, tests can't use an HTTPS server that serves a valid HTTPS
    // response. Check if the URL is treated as secure for testing purposes.
    if (g_form_source_url_port_treated_as_secure_for_insecure_form_testing &&
        source_url.IntPort() !=
            g_form_source_url_port_treated_as_secure_for_insecure_form_testing) {
      return false;
    }
#else
    // On non-iOS platforms, tests should use a proper HTTPS server.
    return false;
#endif
  }

  return IsInsecureFormAction(action_url);
}

bool IsInsecureFormAction(const GURL& action_url) {
#if BUILDFLAG(IS_IOS)
  // On iOS eg tests, test server serves responses at 127.0.0.1 which is
  // normally a trustworthy URL.
  // Check if the URL is treated as insecure for testing purposes.
  if (g_form_action_url_port_treated_as_insecure_for_insecure_form_testing &&
      action_url.IntPort() ==
          g_form_action_url_port_treated_as_insecure_for_insecure_form_testing) {
    return true;
  }
#endif
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
