// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace client_certificates::features {

// Controls whether the management of a client certificate for the current user
// is enabled or not (still requires the policy to be enabled).
BASE_DECLARE_FEATURE(kManagedClientCertificateForUserEnabled);

// Return true if the managed user's client cert feature is enabled.
bool IsManagedClientCertificateForUserEnabled();

// Controls whether the management of a client certificate for the browser
// is enabled or not (still requires the policy to be enabled).
BASE_DECLARE_FEATURE(kManagedBrowserClientCertificateEnabled);

// Return true if the managed browser's client cert feature is enabled.
bool IsManagedBrowserClientCertificateEnabled();

// Controls whether user client certs storage relies on prefs or LevelDB.
BASE_DECLARE_FEATURE(kManagedUserClientCertificateInPrefs);

// Return true if the managed user certificate should be stored in prefs.
bool IsManagedUserClientCertificateInPrefsEnabled();

#if BUILDFLAG(IS_WIN)
// Controls whether Windows software keys are enabled or not.
BASE_DECLARE_FEATURE(kWindowsSoftwareKeysEnabled);

// Return true if Windows software keys are enabled.
bool AreWindowsSoftwareKeysEnabled();
#endif  // BUILDFLAG(IS_WIN)

}  // namespace client_certificates::features

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_FEATURES_H_
