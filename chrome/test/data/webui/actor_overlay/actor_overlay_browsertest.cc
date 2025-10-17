// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

class ActorOverlayMochaTest : public WebUIMochaBrowserTest {
 protected:
  ActorOverlayMochaTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActorUi, {{features::kGlicActorUiOverlayName, "true"}});
    set_test_loader_host(chrome::kChromeUIActorOverlayHost);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorOverlayMochaTest, Scrim) {
  RunTest("actor_overlay/actor_overlay_test.js", "runMochaSuite('Scrim')");
}

IN_PROC_BROWSER_TEST_F(ActorOverlayMochaTest, MagicCursor) {
  RunTest("actor_overlay/actor_overlay_test.js",
          "runMochaSuite('MagicCursor')");
}

IN_PROC_BROWSER_TEST_F(ActorOverlayMochaTest, BorderGlow) {
  RunTest("actor_overlay/actor_overlay_test.js", "runMochaSuite('BorderGlow')");
}

}  // namespace
