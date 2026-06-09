// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/internal/skills_service_impl.h"
#include "components/skills/public/skill_metrics.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "7695055";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsPageElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementEnabled);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementOpen);
}  // namespace
class SkillsPageInteractiveUITest : public InteractiveBrowserTest {
 public:
  SkillsPageInteractiveUITest() = default;
  ~SkillsPageInteractiveUITest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kSkillsEnabled}, {});
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
        profile->GetPrefs(),
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile), chrome::GetChannel(),
        DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
  }

  InteractiveTestApi::MultiStep WaitForElementEvent(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element,
      WebContentsInteractionTestUtil::StateChange::Type type) {
    StateChange state_change;
    state_change.type = type;
    state_change.event = kElementEvent;
    state_change.where = element;
    return WaitForStateChange(contents_id, state_change);
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

  InteractiveTestApi::MultiStep WaitForElementOpen(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    StateChange element_visible;
    element_visible.type = WebContentsInteractionTestUtil::StateChange::Type::
        kExistsAndConditionTrue;
    element_visible.event = kElementOpen;
    element_visible.where = element;
    element_visible.test_function = "(el) => el.open === true";
    return WaitForStateChange(contents_id, element_visible);
  }

  InteractiveTestApi::MultiStep CheckToastIsShowing(ToastId toast_id) {
    return PollUntil(
        [this, toast_id]() {
          auto* controller = browser()->GetFeatures().toast_controller();
          return controller && controller->IsShowingToast() &&
                 controller->GetCurrentToastId() == toast_id;
        },
        "polling until toast is showing");
  }

  InteractiveTestApi::MultiStep AddUserSkillCard() {
    const InteractiveBrowserWindowTestApi::DeepQuery kAddButtonQuery{
        "skills-app", "user-skills-page", "cr-button#addSkillButton"};
    const InteractiveBrowserWindowTestApi::DeepQuery kNameInputQuery{
        "skills-dialog-app", "cr-input#nameText"};
    const InteractiveBrowserWindowTestApi::DeepQuery kInstructionsInputQuery{
        "skills-dialog-app", "textarea#instructionsText"};
    const InteractiveBrowserWindowTestApi::DeepQuery kSaveButtonQuery{
        "skills-dialog-app", "cr-button#saveButton"};
    const InteractiveBrowserWindowTestApi::DeepQuery kNewSkillCardQuery{
        "skills-app", "user-skills-page", "skill-card"};
    return Steps(
        WaitForElementEvent(
            kSkillsPageElementId, kAddButtonQuery,
            WebContentsInteractionTestUtil::StateChange::Type::kExists),
        ClickElement(kSkillsPageElementId, kAddButtonQuery),
        InstrumentNonTabWebView(
            kSkillsDialogElementId,
            skills::SkillsDialogView::kSkillsDialogElementId),
        Do([this]() {
          histogram_tester_.ExpectUniqueSample(
              "Skills.Dialog.Creation.ManagementPage.Blank.Action",
              skills::SkillsDialogAction::kOpened, 1);
        }),
        // Create a new skill.
        WaitForElementEvent(
            kSkillsDialogElementId, kNameInputQuery,
            WebContentsInteractionTestUtil::StateChange::Type::kExists),
        ExecuteJsAt(kSkillsDialogElementId, kNameInputQuery,
                    "el => {"
                    "  el.value = 'My New Skill';"
                    "  el.dispatchEvent(new Event('input', { bubbles: true }));"
                    "}"),
        WaitForElementEvent(
            kSkillsDialogElementId, kInstructionsInputQuery,
            WebContentsInteractionTestUtil::StateChange::Type::kExists),
        ExecuteJsAt(kSkillsDialogElementId, kInstructionsInputQuery,
                    "el => {"
                    "  el.value = 'Instructions';"
                    "  el.dispatchEvent(new Event('input', { bubbles: true }));"
                    "}"),
        WaitForElementEnabled(kSkillsDialogElementId, kSaveButtonQuery),
        ClickElement(kSkillsDialogElementId, kSaveButtonQuery,
                     ExecuteJsMode::kFireAndForget),
        WaitForHide(skills::SkillsDialogView::kSkillsDialogElementId),
        Do([this]() {
          histogram_tester_.ExpectBucketCount(
              "Skills.Dialog.Creation.ManagementPage.Blank.Action",
              skills::SkillsDialogAction::kSaved, 1);
        }),
        WaitForElementEvent(
            kSkillsPageElementId, kNewSkillCardQuery,
            WebContentsInteractionTestUtil::StateChange::Type::kExists));
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
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SkillsPageInteractiveUITest, UndoFromDeletionFlow) {
  const InteractiveBrowserWindowTestApi::DeepQuery kMenuButtonQuery{
      "skills-app", "user-skills-page", "skill-card",
      "cr-icon-button#moreButton"};
  const InteractiveBrowserWindowTestApi::DeepQuery kMenuDropdownQuery{
      "skills-app", "user-skills-page", "skill-card", "cr-action-menu#menu"};
  const InteractiveBrowserWindowTestApi::DeepQuery kDeleteButtonQuery{
      "skills-app", "user-skills-page", "skill-card", "cr-button#deleteButton"};
  const InteractiveBrowserWindowTestApi::DeepQuery kNewSkillCardQuery{
      "skills-app", "user-skills-page", "skill-card"};

  SignIn("testskills@gmail.com");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  RunTestSequence(
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)
                         .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      AddUserSkillCard(), ClickElement(kSkillsPageElementId, kMenuButtonQuery),
      WaitForElementOpen(kSkillsPageElementId, kMenuDropdownQuery),
      ClickElement(kSkillsPageElementId, kDeleteButtonQuery),
      WaitForElementEvent(
          kSkillsPageElementId, kNewSkillCardQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kDoesNotExist),
      CheckToastIsShowing(ToastId::kSkillDeleted),
      // Pressing the toast action button here is the undo.
      PressButton(toasts::ToastView::kToastActionButton),
      // The card should show again on the UI after the undo.
      WaitForElementEvent(
          kSkillsPageElementId, kNewSkillCardQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kExists));
}

IN_PROC_BROWSER_TEST_F(SkillsPageInteractiveUITest, DialogZoomModeDisabled) {
  SignIn("testskills@gmail.com");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);

  const InteractiveBrowserWindowTestApi::DeepQuery kAddButtonQuery{
      "skills-app", "user-skills-page", "cr-button#addSkillButton"};

  RunTestSequence(
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)
                         .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      WaitForElementEvent(
          kSkillsPageElementId, kAddButtonQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kExists),
      ClickElement(kSkillsPageElementId, kAddButtonQuery),
      InstrumentNonTabWebView(kSkillsDialogElementId,
                              skills::SkillsDialogView::kSkillsDialogElementId),
      Do([this]() {
        views::View* web_view =
            views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                skills::SkillsDialogView::kSkillsDialogElementId,
                BrowserView::GetBrowserViewForBrowser(browser())
                    ->GetElementContext());
        EXPECT_TRUE(web_view);
        content::WebContents* web_contents =
            static_cast<skills::SkillsDialogView*>(web_view->parent())
                ->web_contents();
        EXPECT_TRUE(web_contents);
        zoom::ZoomController* zoom_controller =
            zoom::ZoomController::FromWebContents(web_contents);
        EXPECT_TRUE(zoom_controller);
        EXPECT_EQ(zoom_controller->zoom_mode(),
                  zoom::ZoomController::ZOOM_MODE_DISABLED);
      }));
}

class SkillsPageScreenshotInteractiveUITest
    : public SkillsPageInteractiveUITest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsDarkMode() const { return GetParam(); }
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
};

INSTANTIATE_TEST_SUITE_P(All,
                         SkillsPageScreenshotInteractiveUITest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "DarkMode" : "LightMode";
                         });

IN_PROC_BROWSER_TEST_P(SkillsPageScreenshotInteractiveUITest, ErrorStatePage) {
  std::string screenshot_name =
      IsDarkMode() ? "error_state_dark" : "error_state_light";
  const InteractiveBrowserWindowTestApi::DeepQuery kErrorPageQuery{
      "skills-app", "error-page"};
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)),
      WaitForElementEvent(
          kSkillsPageElementId, kErrorPageQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kExists),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
  histogram_tester_.ExpectBucketCount(
      "Skills.Management.ErrorPage.Action",
      skills::mojom::SkillsManagementAction::kPageOpened, 1);
}

IN_PROC_BROWSER_TEST_P(SkillsPageScreenshotInteractiveUITest, ZeroStatePage) {
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
}

IN_PROC_BROWSER_TEST_P(SkillsPageScreenshotInteractiveUITest, NarrowPage) {
  const InteractiveBrowserWindowTestApi::DeepQuery kHamburgerMenuQuery{
      "skills-app", "cr-toolbar#toolbar", "#menuButton"};

  const InteractiveBrowserWindowTestApi::DeepQuery kDrawerQuery{
      "skills-app", "cr-drawer#drawer"};

  std::string screenshot_name = IsDarkMode() ? "narrow_dark" : "narrow_light";
  SignIn("testskills@gmail.com");
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      Do([this]() {
        browser()->GetWindow()->SetBounds(gfx::Rect(0, 0, 400, 800));
      }),
      OpenSkillsPage(GURL(chrome::kChromeUISkillsURL)
                         .Resolve(chrome::kChromeUISkillsYourSkillsPath)),
      WaitForElementEvent(
          kSkillsPageElementId, kHamburgerMenuQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kExists),
      ExecuteJsAt(kSkillsPageElementId, kHamburgerMenuQuery,
                  "(el) => el.click()"),
      WaitForElementEvent(
          kSkillsPageElementId, kDrawerQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kExists),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
}

IN_PROC_BROWSER_TEST_P(SkillsPageScreenshotInteractiveUITest, YourSkillsPage) {
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
      AddUserSkillCard(),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
  histogram_tester_.ExpectBucketCount(
      "Skills.Management.YourSkills.Action",
      skills::mojom::SkillsManagementAction::kPageOpened, 1);
}

IN_PROC_BROWSER_TEST_P(SkillsPageScreenshotInteractiveUITest,
                       BrowseSkillsPage) {
  skills::proto::SkillsList skills_list;
  skills::proto::Skill* skill = skills_list.add_skills();
  skill->set_id("123");
  skill->set_name("Look for socks");
  skill->set_category("Test Category");
  skill->set_description("Look for some socks");
  skill->set_icon("🧦");
  skill->set_prompt("Look for some socks");
  skill->set_image_url("https://example.com/image.png");

  skills::proto::Skill* skill2 = skills_list.add_skills();
  skill2->set_id("345");
  skill2->set_name("Find frog photos");
  skill2->set_category("Learning");
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
      WaitForElementEvent(
          kSkillsPageElementId, kSkillCardQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kExists),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
  histogram_tester_.ExpectBucketCount(
      "Skills.Management.BrowseSkills.Action",
      skills::mojom::SkillsManagementAction::kPageOpened, 1);
}

IN_PROC_BROWSER_TEST_P(SkillsPageScreenshotInteractiveUITest,
                       BrowseSkillsPageSubHeaders) {
  skills::proto::SkillsList skills_list;
  skills::proto::Skill* skill = skills_list.add_skills();
  skill->set_id("123");
  skill->set_name("Look for socks");
  skill->set_category("Partnerships");
  skill->set_description("Look for some socks");
  skill->set_icon("🧦");
  skill->set_prompt("Look for some socks");
  skill->set_image_url("https://example.com/image.png");

  skills::proto::Skill* skill2 = skills_list.add_skills();
  skill2->set_id("345");
  skill2->set_name("Find frog photos");
  skill2->set_category("Top Pick");
  skill2->set_description("Find a cute frog photo");
  skill2->set_icon("🐸");
  skill2->set_prompt("Find a cute frog photo");

  // Add topics.
  skills::proto::TopicInfo* topic1 = skills_list.add_topics_info_list();
  topic1->set_category_name("Partnerships");
  topic1->set_display_name("Partner Spotlight");

  skills::proto::TopicInfo* topic2 = skills_list.add_topics_info_list();
  topic2->set_category_name("Top Pick");
  topic2->set_display_name("Selected By Chrome");

  std::string response_data;
  ASSERT_TRUE(skills_list.SerializeToString(&response_data));

  GURL expected_url(skills::kSkillsDownloaderGstaticUrl);
  test_url_loader_factory_.AddResponse(expected_url.spec(), response_data,
                                       net::HTTP_OK);

  const InteractiveBrowserWindowTestApi::DeepQuery kSkillCardQuery{
      "skills-app", "discover-skills-page", "skill-card"};

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
      WaitForElementEvent(
          kSkillsPageElementId, kSkillCardQuery,
          WebContentsInteractionTestUtil::StateChange::Type::kExists),
      Screenshot(kSkillsPageElementId,
                 /*screenshot_name=*/screenshot_name,
                 /*baseline_cl=*/kScreenshotBaselineCL));
}
