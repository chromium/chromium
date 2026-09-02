// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process_death_test_mixin.h"

#include "chrome/test/base/testing_browser_process.h"

namespace chrome_test_utils {

TestingBrowserProcessDeathTestMixin::TestingBrowserProcessDeathTestMixin() {
  if (!TestingBrowserProcess::GetGlobal()) {
    TestingBrowserProcess::CreateInstance();
  }
}

}  // namespace chrome_test_utils
