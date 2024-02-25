// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PREFS_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PREFS_H_

class PrefRegistrySimple;

namespace client_certificates {

namespace prefs {
// Pref to which the "ProvisionManagedClientCertificateForUserPrefs" policy is
// mapped.
extern const char kProvisionManagedClientCertificateForUserPrefs[];
}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PREFS_H_
