// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/chrome_net_log.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "net/log/net_log_util.h"

namespace net_log {

std::unique_ptr<base::Value> GetConstantsForNetLog(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string) {
  std::unique_ptr<base::DictionaryValue> constants_dict =
      net::GetNetConstants();
  DCHECK(constants_dict);

  auto platform_dict =
      GetPlatformConstantsForNetLog(command_line_string, channel_string);
  if (platform_dict)
    constants_dict->MergeDictionary(platform_dict.get());
  return constants_dict;
}

std::unique_ptr<base::DictionaryValue> GetPlatformConstantsForNetLog(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string) {
  auto constants_dict = std::make_unique<base::DictionaryValue>();

  // Add a dictionary with the version of the client and its command line
  // arguments.
  auto dict = std::make_unique<base::DictionaryValue>();

  // We have everything we need to send the right values.
  dict->SetString("name", version_info::GetProductName());
  dict->SetString("version", version_info::GetVersionNumber());
  dict->SetString("cl", version_info::GetLastChange());
  dict->SetString("version_mod", channel_string);
  dict->SetString("official",
                  version_info::IsOfficialBuild() ? "official" : "unofficial");
  std::string os_type = base::StringPrintf(
      "%s: %s (%s)", base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str(),
      base::SysInfo::OperatingSystemArchitecture().c_str());
  dict->SetString("os_type", os_type);
  dict->SetString("command_line", command_line_string);

  constants_dict->Set("clientInfo", std::move(dict));

  return constants_dict;
}

}  // namespace net_log
