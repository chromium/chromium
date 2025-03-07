// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/install_static/test/scoped_install_details.h"

#include <utility>

#include "base/check_op.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"

namespace install_static {

ScopedInstallDetails::ScopedInstallDetails(bool system_level,
                                           int install_mode_index) {
  std::unique_ptr<PrimaryInstallDetails> details(
      std::make_unique<PrimaryInstallDetails>());
  const InstallConstants* mode = &kInstallModes[install_mode_index];
  details->set_mode(mode);
  details->set_channel(mode->default_channel_name);
  details->set_system_level(system_level);
  these_details_ = details.get();
  previous_details_ = InstallDetails::Swap(std::move(details));
}

ScopedInstallDetails::ScopedInstallDetails(
    std::unique_ptr<InstallDetails> details) {
  these_details_ = details.get();
  previous_details_ = InstallDetails::Swap(std::move(details));
}

ScopedInstallDetails::~ScopedInstallDetails() {
  // Swap the previous details back in, destroying the details created by
  // this scoped object.
  std::unique_ptr<const InstallDetails> details =
      InstallDetails::Swap(std::move(previous_details_));
  // Be sure that no intervening InstallDetails instances snuck in.
  DCHECK_EQ(these_details_, details.get());
}

}  // namespace install_static
