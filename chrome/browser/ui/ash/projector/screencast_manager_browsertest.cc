// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ScreencastManagerTest : public InProcessBrowserTest {
 protected:
  ScreencastManager& screencast_manager() { return screencast_manager_; }

 private:
  ScreencastManager screencast_manager_;
};

}  // namespace ash
