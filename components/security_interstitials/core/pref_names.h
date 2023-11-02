// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_PREF_NAMES_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_PREF_NAMES_H_

namespace prefs {

// Stores counts and timestamps of SSL certificate errors that have occurred.
// When the same error recurs within some period of time, a message is added to
// the SSL interstitial.
extern const char kRecurrentSSLInterstitial[];

// Boolean pref used to control whether mixed forms (forms on HTTPS sites that
// submit over HTTPS) should trigger an on submit warning interstitial. If
// enabled a warning bubble will also show below the form fields and autofill
// will be disabled.
extern const char kMixedFormsWarningsEnabled[];

// A list pref used to enumerate hostnames that should never be identified as
// possible spoofy lookalike domains. This prevents both the lookalike
// interstitial and safety tips from displaying.
extern const char kLookalikeWarningAllowlistDomains[];

}  // namespace prefs

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_PREF_NAMES_H_
