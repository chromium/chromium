// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/features.h"

#if BUILDFLAG(IS_WIN)
#include "crypto/features.h"
#endif  // BUILDFLAG(IS_WIN)

namespace client_certificates::features {

BASE_FEATURE(kManagedClientCertificateForUserEnabled,
             "ManagedClientCertificateForUserEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsManagedClientCertificateForUserEnabled() {
  return base::FeatureList::IsEnabled(kManagedClientCertificateForUserEnabled);
}

BASE_FEATURE(kManagedBrowserClientCertificateEnabled,
             "ManagedBrowserClientCertificateEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsManagedBrowserClientCertificateEnabled() {
  return base::FeatureList::IsEnabled(kManagedBrowserClientCertificateEnabled);
}

BASE_FEATURE(kManagedUserClientCertificateInPrefs,
             "ManagedUserClientCertificateInPrefs",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsManagedUserClientCertificateInPrefsEnabled() {
  return base::FeatureList::IsEnabled(kManagedUserClientCertificateInPrefs);
}

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kWindowsSoftwareKeysEnabled,
             "WindowsSoftwareKeysEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool AreWindowsSoftwareKeysEnabled() {
  // Windows Software keys depend on a fix in the //crypto layer.
  return base::FeatureList::IsEnabled(
             crypto::features::kIsHardwareBackedFixEnabled) &&
         base::FeatureList::IsEnabled(kWindowsSoftwareKeysEnabled);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace client_certificates::features
