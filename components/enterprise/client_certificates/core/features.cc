// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/features.h"

namespace client_certificates::features {

BASE_FEATURE(kManagedBrowserClientCertificateEnabled,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsManagedBrowserClientCertificateEnabled() {
  return base::FeatureList::IsEnabled(kManagedBrowserClientCertificateEnabled);
}

BASE_FEATURE(kEnableClientCertificateProvisioningOnAndroid,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsClientCertificateProvisioningOnAndroidEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableClientCertificateProvisioningOnAndroid);
}

BASE_FEATURE(kManagedUserClientCertificateInPrefs,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsManagedUserClientCertificateInPrefsEnabled() {
  return base::FeatureList::IsEnabled(kManagedUserClientCertificateInPrefs);
}

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kWindowsSoftwareKeysEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

bool AreWindowsSoftwareKeysEnabled() {
  return base::FeatureList::IsEnabled(kWindowsSoftwareKeysEnabled);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace client_certificates::features
