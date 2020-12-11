// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALL_STATIC_TEST_SCOPED_INSTALL_DETAILS_H_
#define CHROME_INSTALL_STATIC_TEST_SCOPED_INSTALL_DETAILS_H_

#include <memory>

#include "base/macros.h"

namespace install_static {

class InstallDetails;

// A facility for tests to register an InstallDetails for the duration of a
// test harness or test run.
class ScopedInstallDetails {
 public:
  // Installs an InstallDetails instance that will report the install as being
  // at |system_level| and of mode |install_mode_index| (an InstallConstantIndex
  // value) of the current brand; see ../install_modes.h for details.
  // TODO(grt): replace bool and int with more obvious types (e.g., enum).
  explicit ScopedInstallDetails(bool system_level = false,
                                int install_mode_index = 0);
  ~ScopedInstallDetails();

 private:
  // A raw pointer to the InstallDetails instance created by this object. This
  // is used only to assert that no intervening instances were swapped in yet
  // not restored during the lifetime of this object.
  const InstallDetails* these_details_ = nullptr;

  // The module's InstallDetails instance prior to the creation of this object.
  // This instance will be swapped back into place when this object is
  // destroyed.
  std::unique_ptr<const InstallDetails> previous_details_;

  DISALLOW_COPY_AND_ASSIGN(ScopedInstallDetails);
};

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_TEST_SCOPED_INSTALL_DETAILS_H_
