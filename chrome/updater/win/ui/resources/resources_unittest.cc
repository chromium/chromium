// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/resources/resources.grh"

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

// Tests that the UI resources are linked in the unit tests to allow writing
// of unit tests for the UI.
TEST(Updater, UpdaterTestsContainResources) {
  ASSERT_TRUE(::FindResourceW(NULL, MAKEINTRESOURCE(IDD_YES_NO), RT_DIALOG));
}

}  // namespace updater
