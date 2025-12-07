// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/public/reboot_shlib.h"

#include <stdlib.h>  // abort()
#define NOTREACHED() abort()

namespace chromecast {

void RebootShlib::Initialize(const std::vector<std::string>& /* argv */) {}

void RebootShlib::Finalize() {}

bool RebootShlib::IsSupported() {
  return false;
}

bool RebootShlib::IsRebootSourceSupported(
    RebootShlib::RebootSource /* reboot_source */) {
  return false;
}

bool RebootShlib::RebootNow(RebootShlib::RebootSource /* reboot_source */) {
  return false;
}

bool RebootShlib::IsFdrForNextRebootSupported() {
  return false;
}

void RebootShlib::SetFdrForNextReboot() {
  NOTREACHED();
}

bool RebootShlib::IsOtaForNextRebootSupported() {
  return false;
}

void RebootShlib::SetOtaForNextReboot() {
  NOTREACHED();
}

bool RebootShlib::IsClearOtaForNextRebootSupported() {
  return false;
}

void RebootShlib::ClearOtaForNextReboot() {
  NOTREACHED();
}

}  // namespace chromecast
