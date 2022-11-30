// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_INTRANET_REDIRECTOR_STATE_H_
#define COMPONENTS_OMNIBOX_BROWSER_INTRANET_REDIRECTOR_STATE_H_

#include "components/prefs/pref_service.h"

namespace omnibox {

// Settings values for intranet redirection and did-you-mean infobars.
enum class IntranetRedirectorBehavior {
  // Both redirect checks and the did-you-mean infobars are disabled.
  // Default value in M97 and above.
  DISABLE_FEATURE = 1,
  // Checks are disabled for all domains but did-you-mean infobars will be
  // shown for single-word queries. This is useful for cases where enterprises
  // are certain DNS requests will not be hijacked.
  DISABLE_INTERCEPTION_CHECKS_ENABLE_INFOBARS = 2,
  // Enable both interception checks and the infobar.
  // Default value prior to M97.
  ENABLE_INTERCEPTION_CHECKS_AND_INFOBARS = 3,
};

// Returns the current behavior of the redirect detector feature.
// Defined by policy for enterprises; disabled for non-enterprises.
IntranetRedirectorBehavior GetInterceptionChecksBehavior(
    const PrefService* prefs);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_INTRANET_REDIRECTOR_STATE_H_
