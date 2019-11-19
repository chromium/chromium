// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
#define COMPONENTS_NET_LOG_CHROME_NET_LOG_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace net_log {

// Returns all the constants to include in NetLog files. This includes both
// platform-specific details (GetPlatformConstantsForNetLog()) as well as the
// basic src/net constants (net::GetNetConstants()) for things like symbolic
// names of error codes.
//
// Safe to call on any thread.
std::unique_ptr<base::Value> GetConstantsForNetLog(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string);

// Returns constants to include in NetLog files for debugging purposes, which
// includes information such as:
//
//  * The version and build of Chrome
//  * The command line arguments Chrome was launched with
//  * The operating system version
//
//  Safe to call on any thread.
std::unique_ptr<base::DictionaryValue> GetPlatformConstantsForNetLog(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string);

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
