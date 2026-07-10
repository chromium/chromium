// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/color_internals/url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

using ColorInternalsBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(ColorInternalsBrowserTest, All) {
  set_test_loader_host(ash::kChromeUIColorInternalsHost);
  RunTestWithoutTestLoader("chromeos/color_internals/color_internals_test.js",
                           "mocha.run()");
}

}  // namespace
}  // namespace ash
