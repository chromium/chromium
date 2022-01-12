// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_KEYSTONE_KSADMIN_H_
#define CHROME_UPDATER_MAC_KEYSTONE_KSADMIN_H_

#include <string>

#include "base/containers/flat_map.h"

namespace updater {

namespace ksadmin_internal {

// Exports the function for testing purpose.
base::flat_map<std::string, std::string> ParseCommandLine(int argc,
                                                          const char* argv[]);

}  // namespace ksadmin_internal

int KSAdminAppMain(int argc, const char* argv[]);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_KEYSTONE_KSADMIN_H_
