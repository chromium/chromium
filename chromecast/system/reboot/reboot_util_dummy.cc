// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/system/reboot/reboot_util.h"

namespace chromecast {

// static
void RebootUtil::Initialize(const std::vector<std::string>& argv) {
  RebootShlib::Initialize(argv);
}

// static
void RebootUtil::Finalize() {
  RebootShlib::Finalize();
}

// static
RebootShlib::RebootSource RebootUtil::GetLastRebootSource() {
    return RebootShlib::RebootSource::UNKNOWN;
}

// static
bool RebootUtil::SetNextRebootSource(
    RebootShlib::RebootSource /* reboot_source */) {
  return false;
}

}  // namespace chromecast
