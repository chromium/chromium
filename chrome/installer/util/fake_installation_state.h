// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_FAKE_INSTALLATION_STATE_H_
#define CHROME_INSTALLER_UTIL_FAKE_INSTALLATION_STATE_H_

#include "base/files/file_path.h"
#include "base/version.h"
#include "chrome/installer/util/fake_product_state.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

// An InstallationState helper for use by unit tests.
class FakeInstallationState : public InstallationState {
 public:
  // Takes ownership of |version|.
  void AddChrome(bool system_install, base::Version* version) {
    FakeProductState chrome_state;
    chrome_state.set_version(version);
    base::FilePath setup_exe(GetDefaultChromeInstallPath(system_install));
    setup_exe = setup_exe.AppendASCII(version->GetString())
                    .Append(kInstallerDir)
                    .Append(kSetupExe);
    chrome_state.SetUninstallProgram(setup_exe);
    chrome_state.AddUninstallSwitch(switches::kUninstall);
    SetProductState(system_install, chrome_state);
  }

  void SetProductState(bool system_install, const ProductState& product_state) {
    GetProduct(system_install)->CopyFrom(product_state);
  }

 protected:
  ProductState* GetProduct(bool system_install) {
    return system_install ? &system_chrome_ : &user_chrome_;
  }
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_FAKE_INSTALLATION_STATE_H_
