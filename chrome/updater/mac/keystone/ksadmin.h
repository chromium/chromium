// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_KEYSTONE_KSADMIN_H_
#define CHROME_UPDATER_MAC_KEYSTONE_KSADMIN_H_

#include <map>
#include <string>

namespace updater {

// Exports the function for testing purpose.
std::map<std::string, std::string> ParseCommandLine(int argc,
                                                    const char* argv[]);

int KSAdminAppMain(int argc, const char* argv[]);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_KEYSTONE_KSADMIN_H_
