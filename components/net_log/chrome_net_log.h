// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
#define COMPONENTS_NET_LOG_CHROME_NET_LOG_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/values.h"

namespace net_log {

// Returns constants to include in NetLog files including information such as:
//
//  * The version and build of Chrome
//  * The command line arguments Chrome was launched with
//  * The operating system version
//
//  Safe to call on any thread.
base::Value::Dict GetPlatformConstantsForNetLog(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string);

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
