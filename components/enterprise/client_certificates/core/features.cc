// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/features.h"

namespace client_certificates::features {

BASE_FEATURE(kEnableClientCertificateProvisioningOnAndroid,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsClientCertificateProvisioningOnAndroidEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableClientCertificateProvisioningOnAndroid);
}

BASE_FEATURE(kEnableClientCertificateProvisioningOnIOS,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsClientCertificateProvisioningOnIOSEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableClientCertificateProvisioningOnIOS);
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

BASE_FEATURE(kWindowsTpmTls13Check, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsWindowsTpmTls13CheckEnabled() {
  return base::FeatureList::IsEnabled(kWindowsTpmTls13Check);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace client_certificates::features
