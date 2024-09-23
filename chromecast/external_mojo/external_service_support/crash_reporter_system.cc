// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/external_mojo/external_service_support/crash_reporter_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"

namespace chromecast {
namespace external_service_support {

// static
void CrashReporterClient::Init() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  crashpad::CrashpadInfo* crashpad_info =
      crashpad::CrashpadInfo::GetCrashpadInfo();
  if (command_line->HasSwitch(switches::kDisableCrashpadForwarding)) {
    LOG(INFO) << "Crashpad forwarding disabled";
    crashpad_info->set_system_crash_reporter_forwarding(
        crashpad::TriState::kDisabled);
  } else {
    crashpad_info->set_system_crash_reporter_forwarding(
        crashpad::TriState::kEnabled);
  }
}

}  // namespace external_service_support
}  // namespace chromecast
