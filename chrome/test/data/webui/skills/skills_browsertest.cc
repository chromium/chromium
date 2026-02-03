// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/skills/features.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_enabling.h"
#endif

namespace {

// TODO(b/481023023): Instead of gating all the tests, create an error page and
// have a dedicated test for that.
#if BUILDFLAG(ENABLE_GLIC)
class SkillsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SkillsBrowserTest() { set_test_loader_host(chrome::kChromeUISkillsHost); }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  }

  void TearDownOnMainThread() override {
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
    WebUIMochaBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kSkillsEnabled};
};

IN_PROC_BROWSER_TEST_F(SkillsBrowserTest, SkillsAppPage) {
  RunTest("skills/skills_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(SkillsBrowserTest, SkillsDialogAppPage) {
  RunTest("skills/skills_dialog_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(SkillsBrowserTest, UserSkillsPage) {
  RunTest("skills/user_skills_page_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(SkillsBrowserTest, DiscoverSkillsPage) {
  RunTest("skills/discover_skills_page_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(SkillsBrowserTest, SkillCard) {
  RunTest("skills/card_test.js", "mocha.run();");
}
#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace
