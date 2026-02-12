// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/skills/features.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_enabling.h"
#endif

namespace {
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7568028";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsPageElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
}  // namespace

class SkillsPageInteractiveUITest : public InteractiveBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  SkillsPageInteractiveUITest() = default;
  ~SkillsPageInteractiveUITest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    if (IsDarkMode()) {
      enabled_features = {features::kSkillsEnabled,
                          blink::features::kForceWebContentsDarkMode};
    } else {
      enabled_features = {features::kSkillsEnabled};
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
    InteractiveBrowserTest::SetUp();
  }

  bool IsDarkMode() const { return GetParam(); }

  InteractiveTestApi::MultiStep WaitForElementExists(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_exists;
    element_exists.type =
        WebContentsInteractionTestUtil::StateChange::Type::kExists;
    element_exists.event = kElementExists;
    element_exists.where = element;
    return WaitForStateChange(contents_id, element_exists);
  }

  InteractiveTestApi::MultiStep OpenSkillsPage(const GURL& url) {
    return Steps(InstrumentTab(kSkillsPageElementId),
                 NavigateWebContents(kSkillsPageElementId, url),
                 WaitForWebContentsReady(kSkillsPageElementId, url));
  }

  void SignIn(const std::string& email) {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, email,
                                        signin::ConsentLevel::kSync);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SkillsPageInteractiveUITest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "DarkMode" : "LightMode";
                         });

IN_PROC_BROWSER_TEST_P(SkillsPageInteractiveUITest, ErrorStatePage) {
  std::string screenshot_name =
      IsDarkMode() ? "error_state_dark" : "error_state_light";
  RunTestSequence(SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshots not supported in all testing environments."),
                  OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)),
                  Screenshot(kSkillsPageElementId,
                             /*screenshot_name=*/screenshot_name,
                             /*baseline_cl=*/kScreenshotBaselineCL));
}

IN_PROC_BROWSER_TEST_P(SkillsPageInteractiveUITest, ZeroStatePage) {
#if BUILDFLAG(ENABLE_GLIC)
  std::string screenshot_name =
      IsDarkMode() ? "zero_state_dark" : "zero_state_light";
  SignIn("testskills@gmail.com");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)
                         .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
#endif
}

IN_PROC_BROWSER_TEST_P(SkillsPageInteractiveUITest, NarrowPage) {
  const InteractiveBrowserWindowTestApi::DeepQuery kHamburgerMenuQuery{
      "skills-app", "cr-toolbar#toolbar", "#menuButton"};

  const InteractiveBrowserWindowTestApi::DeepQuery kDrawerQuery{
      "skills-app", "cr-drawer#drawer"};

#if BUILDFLAG(ENABLE_GLIC)
  std::string screenshot_name = IsDarkMode() ? "narrow_dark" : "narrow_light";
  SignIn("testskills@gmail.com");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Do([this]() {
        browser()->window()->SetBounds(gfx::Rect(0, 0, 400, 800));
      }),
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)
                         .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      WaitForElementExists(kSkillsPageElementId, kHamburgerMenuQuery),
      ExecuteJsAt(kSkillsPageElementId, kHamburgerMenuQuery,
                  "(el) => el.click()"),
      WaitForElementExists(kSkillsPageElementId, kDrawerQuery),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
#endif
}
