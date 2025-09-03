// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/util.h"

#include <string>

#include "base/command_line.h"
#include "base/version_info/channel.h"
#include "chromeos/ash/components/channel/channel_info.h"

namespace ash::boca {

namespace {

inline constexpr char kSchoolToolsApiBaseProdUrl[] =
    "https://schooltools-pa.googleapis.com";
inline constexpr char kSchoolToolsServerSwitch[] = "st-server";

}  // namespace

std::string GetSchoolToolsUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (ash::GetChannel() != version_info::Channel::STABLE &&
      ash::GetChannel() != version_info::Channel::BETA &&
      command_line->HasSwitch(kSchoolToolsServerSwitch)) {
    return command_line->GetSwitchValueASCII(kSchoolToolsServerSwitch);
  }
  return kSchoolToolsApiBaseProdUrl;
}

}  // namespace ash::boca
