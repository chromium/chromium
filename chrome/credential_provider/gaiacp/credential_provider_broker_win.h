// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_CREDENTIAL_PROVIDER_BROKER_WIN_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_CREDENTIAL_PROVIDER_BROKER_WIN_H_

#include <string>

#include "chrome/credential_provider/gaiacp/mojom/gaia_credential_provider_win_hid.mojom.h"

#include "base/win/scoped_handle.h"

namespace credential_provider {

class CredentialProviderBrokerWin
    : public gcpw_hid::mojom::GaiaCredentialProviderHidBroker {
 public:
  CredentialProviderBrokerWin();
  ~CredentialProviderBrokerWin() override;

 protected:
  // GcpwHidBroker impl:
  void OpenDevice(const std::u16string& device_path,
                  OpenDeviceCallback callback) override;
};
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_CREDENTIAL_PROVIDER_BROKER_WIN_H_
