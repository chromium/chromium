// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_MODULE_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_MODULE_H_

// Due to windows include file ordering, this needs to remain first.
#include "chrome/credential_provider/gaiacp/stdafx.h"

#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

#include "base/at_exit.h"

namespace base {
class AtExitManager;
}

namespace credential_provider {

// Declaration of Afx module class for this DLL.
class CGaiaCredentialProviderModule
    : public ATL::CAtlDllModuleT<CGaiaCredentialProviderModule> {
 public:
  CGaiaCredentialProviderModule();
  ~CGaiaCredentialProviderModule() override;

  DECLARE_LIBID(LIBID_GaiaCredentialProviderLib)

  // This class implements UpdateRegistryAppId() directly instead of using the
  // the DECLARE_REGISTRY_APPID_RESOURCEID so that it can use additional rgs
  // file variable substitutions.
  static HRESULT WINAPI UpdateRegistryAppId(BOOL do_register) throw();

  // Called from DLL entry point to handle attaching and detaching from
  // processes and threads.
  BOOL DllMain(HINSTANCE hinstance, DWORD reason, LPVOID reserved);

  // Indicates if the instance is running in a test.
  void set_is_testing(bool is_testing) { is_testing_ = is_testing; }

 private:
  std::unique_ptr<base::AtExitManager> exit_manager_;
  bool is_testing_ = false;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_PROVIDER_MODULE_H_
