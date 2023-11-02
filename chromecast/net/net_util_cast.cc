// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/net_util_cast.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/net/net_switches.h"
#include "chromecast/public/cast_sys_info.h"

namespace chromecast {

std::unordered_set<std::string> GetIgnoredInterfaces() {
  std::unordered_set<std::string> ignored_interfaces;
  std::unique_ptr<CastSysInfo> sys_info = CreateSysInfo();
  if (!sys_info->GetApInterface().empty())
    ignored_interfaces.insert(sys_info->GetApInterface());

  // Add interfaces from "netif-to-ignore" switch.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType netifs_to_ignore_str =
      command_line->GetSwitchValueNative(switches::kNetifsToIgnore);
  for (const std::string& netif : base::SplitString(
           netifs_to_ignore_str, ",", base::TRIM_WHITESPACE,
           base::SPLIT_WANT_ALL))
    ignored_interfaces.insert(netif);

  return ignored_interfaces;
}

}  // namespace chromecast
