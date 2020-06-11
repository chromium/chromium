// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SYSTEM_REBOOT_REBOOT_FUCHSIA_H_
#define CHROMECAST_SYSTEM_REBOOT_REBOOT_FUCHSIA_H_

#include <string>
#include <vector>

namespace sys {
class ServiceDirectory;
}  // namespace sys

namespace chromecast {

// Injects a service directory for testing.
void InitializeRebootShlib(const std::vector<std::string>& argv,
                           sys::ServiceDirectory* incoming_directory);

}  // namespace chromecast

#endif  // CHROMECAST_SYSTEM_REBOOT_REBOOT_FUCHSIA_H_
