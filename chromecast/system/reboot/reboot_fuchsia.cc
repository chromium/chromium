// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/reboot_shlib.h"

namespace chromecast {

// RebootShlib implementation:

// static
void RebootShlib::Initialize(const std::vector<std::string>& argv) {}

void RebootShlib::Finalize() {}

// static
bool RebootShlib::IsSupported() {
  return false;
}

// static
// Chromecast devices support all RebootSources
bool RebootShlib::IsRebootSourceSupported(
    RebootShlib::RebootSource /* reboot_source */) {
  return false;
}

// static
// Chromecast devices support all RebootSources
bool RebootShlib::RebootNow(RebootSource /* reboot_source */) {
  // TODO(b/140491587): Implement reboot on Fuchsia.
  return false;
}

// static
bool RebootShlib::IsFdrForNextRebootSupported() {
  return false;
}

// static
void RebootShlib::SetFdrForNextReboot() {}

// static
bool RebootShlib::IsOtaForNextRebootSupported() {
  return false;
}

// static
void RebootShlib::SetOtaForNextReboot() {}

}  // namespace chromecast
