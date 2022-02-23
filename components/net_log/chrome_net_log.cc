// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/chrome_net_log.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

namespace net_log {

std::unique_ptr<base::DictionaryValue> GetPlatformConstantsForNetLog(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string) {
  auto constants_dict = std::make_unique<base::DictionaryValue>();

  // Add a dictionary with the version of the client and its command line
  // arguments.
  base::DictionaryValue dict;

  // We have everything we need to send the right values.
  dict.SetStringKey("name", version_info::GetProductName());
  dict.SetStringKey("version", version_info::GetVersionNumber());
  dict.SetStringKey("cl", version_info::GetLastChange());
  dict.SetStringKey("version_mod", channel_string);
  dict.SetStringKey(
      "official", version_info::IsOfficialBuild() ? "official" : "unofficial");
  std::string os_type = base::StringPrintf(
      "%s: %s (%s)", base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str(),
      base::SysInfo::OperatingSystemArchitecture().c_str());
  dict.SetStringKey("os_type", os_type);
#if BUILDFLAG(IS_WIN)
  dict.SetStringKey("command_line", base::WideToUTF8(command_line_string));
#else
  dict.SetStringKey("command_line", command_line_string);
#endif

  constants_dict->SetKey("clientInfo", std::move(dict));

  return constants_dict;
}

}  // namespace net_log
