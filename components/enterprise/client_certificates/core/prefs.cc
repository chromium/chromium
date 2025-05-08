// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/prefs.h"

#include "components/enterprise/client_certificates/core/constants.h"
#include "components/prefs/pref_registry_simple.h"

namespace client_certificates {

namespace prefs {
const char kProvisionManagedClientCertificateForUserPrefs[] =
    "client_certificates.provision_for_user.value";
const char kProvisionManagedClientCertificateForBrowserPrefs[] =
    "client_certificates.provision_for_browser.value";
}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kProvisionManagedClientCertificateForUserPrefs,
      /*default_value=*/0);
  registry->RegisterDictionaryPref(kManagedProfileIdentityName);
  registry->RegisterDictionaryPref(kTemporaryManagedProfileIdentityName);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kProvisionManagedClientCertificateForBrowserPrefs,
      /*default_value=*/0);
  registry->RegisterDictionaryPref(kManagedBrowserIdentityName);
  registry->RegisterDictionaryPref(kTemporaryManagedBrowserIdentityName);
}

}  // namespace client_certificates
