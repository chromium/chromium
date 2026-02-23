// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/internal/skills_service_impl.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
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
constexpr char kScreenshotBaselineCL[] = "7573535";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsPageElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementEnabled);
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

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    skills::SkillsServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&SkillsPageInteractiveUITest::CreateSkillsService,
                            base::Unretained(this)));
    skills::SkillsService* skills_service =
        skills::SkillsServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(skills_service);
    skills_service->SetServiceStatusForTesting(
        skills::SkillsService::ServiceStatus::kReady);
  }

  std::unique_ptr<KeyedService> CreateSkillsService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<skills::SkillsServiceImpl>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
        chrome::GetChannel(),
        DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
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

  InteractiveTestApi::MultiStep WaitForElementEnabled(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_enabled;
    element_enabled.type = WebContentsInteractionTestUtil::StateChange::Type::
        kExistsAndConditionTrue;
    element_enabled.event = kElementEnabled;
    element_enabled.where = element;
    element_enabled.test_function = "(el) => !el.disabled";
    return WaitForStateChange(contents_id, element_enabled);
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

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;

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

IN_PROC_BROWSER_TEST_P(SkillsPageInteractiveUITest, YourSkillsPage) {
  const InteractiveBrowserWindowTestApi::DeepQuery kAddButtonQuery{
      "skills-app", "user-skills-page", "cr-button#addSkillButton"};
  const InteractiveBrowserWindowTestApi::DeepQuery kNameInputQuery{
      "skills-dialog-app", "cr-input#nameText"};
  const InteractiveBrowserWindowTestApi::DeepQuery kDescriptionInputQuery{
      "skills-dialog-app", "textarea#instructionsText"};
  const InteractiveBrowserWindowTestApi::DeepQuery kSaveButtonQuery{
      "skills-dialog-app", "cr-button#saveButton"};
  const InteractiveBrowserWindowTestApi::DeepQuery kNewSkillCardQuery{
      "skills-app", "user-skills-page", "skill-card"};

#if BUILDFLAG(ENABLE_GLIC)
  std::string screenshot_name =
      IsDarkMode() ? "your_skills_dark" : "your_skills_light";
  SignIn("testskills@gmail.com");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)
                         .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      // Click the add button, this should trigger the dialog to open.
      WaitForElementExists(kSkillsPageElementId, kAddButtonQuery),
      ClickElement(kSkillsPageElementId, kAddButtonQuery),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      // Create a new skill.
      WaitForElementExists(kSkillsDialogElementId, kNameInputQuery),
      ExecuteJsAt(kSkillsDialogElementId, kNameInputQuery,
                  "el => {"
                  "  el.value = 'My New Skill';"
                  "  el.dispatchEvent(new Event('input', { bubbles: true }));"
                  "}"),
      WaitForElementExists(kSkillsDialogElementId, kDescriptionInputQuery),
      ExecuteJsAt(kSkillsDialogElementId, kDescriptionInputQuery,
                  "el => {"
                  "  el.value = 'Instructions';"
                  "  el.dispatchEvent(new Event('input', { bubbles: true }));"
                  "}"),
      WaitForElementEnabled(kSkillsDialogElementId, kSaveButtonQuery),
      MoveMouseTo(kSkillsDialogElementId, kSaveButtonQuery), ClickMouse(),
      WaitForHide(skills::SkillsDialogView::kSkillsDialogElementId),
      WaitForElementExists(kSkillsPageElementId, kNewSkillCardQuery),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
#endif
}

IN_PROC_BROWSER_TEST_P(SkillsPageInteractiveUITest, BrowseSkillsPage) {
  skills::proto::SkillsList skills_list;
  skills::proto::Skill* skill = skills_list.add_skills();
  skill->set_id("123");
  skill->set_name("Look for socks");
  skill->set_category("Test Category");
  skill->set_description("Look for some socks");
  skill->set_icon("🧦");
  skill->set_prompt("Look for some socks");

  skills::proto::Skill* skill2 = skills_list.add_skills();
  skill2->set_id("345");
  skill2->set_name("Find frog photos");
  skill2->set_category("Top Pick");
  skill2->set_description("Find a cute frog photo");
  skill2->set_icon("🐸");
  skill2->set_prompt("Find a cute frog photo");

  std::string response_data;
  ASSERT_TRUE(skills_list.SerializeToString(&response_data));

  GURL expected_url(skills::kSkillsDownloaderGstaticUrl);
  test_url_loader_factory_.AddResponse(expected_url.spec(), response_data,
                                       net::HTTP_OK);

  const InteractiveBrowserWindowTestApi::DeepQuery kSkillCardQuery{
      "skills-app", "discover-skills-page", "skill-card"};

#if BUILDFLAG(ENABLE_GLIC)
  std::string screenshot_name =
      IsDarkMode() ? "browse_skills_dark" : "browse_skills_light";
  SignIn("testskills@gmail.com");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)
                         .Resolve(chrome::kChromeUISkillsBrowsePath)),
      WaitForElementExists(kSkillsPageElementId, kSkillCardQuery),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
#endif
}
