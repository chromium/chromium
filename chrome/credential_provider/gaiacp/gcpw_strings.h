// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_STRINGS_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_STRINGS_H_

namespace credential_provider {
// Time parameters to control validity of the offline session.
extern const char kKeyLastTokenValid[];
extern const char kKeyValidityPeriodInDays[];
// DEPRECATED
extern const char kKeyLastSuccessfulOnlineLoginMillis[];
// Registry parameters for gcpw.
extern const wchar_t kKeyAcceptTos[];
// Registry parameter controlling whether features related to GEM
// should be enabled / disabled.
extern const wchar_t kKeyEnableGemFeatures[];
extern const char kGaiaSetupPath[];

// URL for the GEM service handling GCPW requests.
extern const wchar_t kDefaultGcpwServiceUrl[];
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_STRINGS_H_
