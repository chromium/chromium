// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"

namespace chromecast {
namespace external_service_support {

// static
void CrashReporterClient::Init() {
  crashpad::CrashpadInfo::GetCrashpadInfo()
      ->set_system_crash_reporter_forwarding(crashpad::TriState::kEnabled);
}

}  // namespace external_service_support
}  // namespace chromecast
