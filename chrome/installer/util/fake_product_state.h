// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_FAKE_PRODUCT_STATE_H_
#define CHROME_INSTALLER_UTIL_FAKE_PRODUCT_STATE_H_

#include <string>
#include "chrome/installer/util/installation_state.h"

namespace installer {

// A ProductState helper for use by unit tests.
class FakeProductState : public ProductState {
 public:
  // Takes ownership of |version|.
  void set_version(base::Version* version) { version_.reset(version); }
  void set_brand(const std::wstring& brand) { brand_ = brand; }
  void set_usagestats(DWORD usagestats) {
    has_usagestats_ = true;
    usagestats_ = usagestats;
  }
  void clear_usagestats() { has_usagestats_ = false; }
  void SetUninstallProgram(const base::FilePath& setup_exe) {
    uninstall_command_ = base::CommandLine(setup_exe);
  }
  void AddUninstallSwitch(const std::string& option) {
    uninstall_command_.AppendSwitch(option);
  }
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_FAKE_PRODUCT_STATE_H_
