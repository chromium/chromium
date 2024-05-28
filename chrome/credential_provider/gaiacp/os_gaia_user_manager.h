// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_GAIA_USER_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_GAIA_USER_MANAGER_H_

#include "base/win/windows_types.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"

namespace credential_provider {
// This class is intended to manage the local OS user account named "gaia" that
// is created by the GCPW installer.
class OSGaiaUserManager {
 public:
  ~OSGaiaUserManager();

  static OSGaiaUserManager* Get();

  // Creates gaia user and returns its sid.
  HRESULT CreateGaiaUser(PSID* out_sid);

  // Changes the gaia user password if its sid has changed since installation.
  HRESULT ChangeGaiaUserPasswordIfNeeded();

  // Set fakes for unit tests.
  void SetFakesForTesting(FakesForTesting* fakes);

 protected:
  OSGaiaUserManager() {}

  // Returns the storage used for the instance pointer.
  static OSGaiaUserManager** GetInstanceStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_GAIA_USER_MANAGER_H_
