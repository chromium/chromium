// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace ttc {

class AiOverlayDialogWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  AiOverlayDialogWebUIBrowserTest() {
    set_test_loader_host(chrome::kChromeUIAiOverlayDialogUntrustedHost);
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(IS_CHROMEOS)
            chromeos::features::kFeatureManagementGlic,
#endif  // BUILDFLAG(IS_CHROMEOS)
            features::kAiOverlayDialog},
        {});
    WebUIMochaBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AiOverlayDialogWebUIBrowserTest, Persona) {
  RunTest("ai_overlay_dialog/persona_test.js", "mocha.run()");
}

}  // namespace ttc
