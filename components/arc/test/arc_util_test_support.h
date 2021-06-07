// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_ARC_UTIL_TEST_SUPPORT_H_
#define COMPONENTS_ARC_TEST_ARC_UTIL_TEST_SUPPORT_H_

#include <string>

namespace base {
struct SystemMemoryInfoKB;
}  // namespace base

namespace arc {

// TODO: Refactor following test functions from arc_util.h/.cc as well.
// - ShouldShowOptInForTesting
// - SetArcAlwaysStartWithoutPlayStoreForTesting
// - SetArcAvailableCommandLineForTesting

// Gets a system memory profile based on file name.
bool GetSystemMemoryInfoForTesting(const std::string& file_name,
                                   base::SystemMemoryInfoKB* mem_info);

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_ARC_UTIL_TEST_SUPPORT_H_
