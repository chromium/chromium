// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/personalization_app_mojom_banned_mocha_test_base.h"
#include "content/public/test/browser_test.h"

namespace ash::personalization_app {

// Tests state management and logic in SeaPen.
using SeaPenControllerTest = PersonalizationAppMojomBannedMochaTestBase;

IN_PROC_BROWSER_TEST_F(SeaPenControllerTest, All) {
  RunTest("chromeos/personalization_app/sea_pen_controller_test.js",
          "mocha.run()");
}

}  // namespace ash::personalization_app
