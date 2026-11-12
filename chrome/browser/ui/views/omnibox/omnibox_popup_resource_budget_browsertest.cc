// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Budgets for the resources WebUI omnibox popups consume per browser window.
// These guard against regressions in popup WebContents, widget and renderer
// process counts; tighten them as optimizations land. Memory is logged, not
// asserted, since it is too noisy to gate on. Run with
// --test-launcher-print-test-stdio=always to see the "OmniboxPopupBudget"
// lines.

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/compositor.h"
#include "ui/views/widget/widget.h"

namespace {

// Classic (or full) popup plus the AIM popup, created eagerly per window.
constexpr size_t kPopupWebContentsPerWindow = 2;
// Widgets are created on first show and, in these configurations, destroyed on
// hide, so a hidden popup with a widget was created eagerly. Which popups are
// showing depends on window focus, which varies by platform, so only hidden
// ones are budgeted.
constexpr size_t kMaxHiddenPopupWidgets = 0;
// AIM-ineligible profiles get only the classic (or full) popup.
constexpr size_t kPopupWebContentsPerAimIneligibleWindow = 1;
// Top-chrome WebUIs of one profile share a renderer process.
constexpr size_t kPopupRenderProcessesPerProfile = 1;

// Enough to show scaling. Each window loads two WebUI documents, which is slow
// on debug and sanitizer builds.
constexpr size_t kExtraWindows = 2;

struct PopupResources {
  std::vector<raw_ptr<content::WebContents>> web_contents;
  std::set<raw_ptr<content::RenderProcessHost>> processes;
  size_t widgets = 0;
  size_t hidden_widgets = 0;
};

LocationBarView* GetLocationBarView(BrowserWindowInterface* browser) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
  return toolbar ? toolbar->location_bar_view() : nullptr;
}

bool IsOmniboxPopup(content::WebContents* contents) {
  return contents->GetVisibleURL().host() == chrome::kChromeUIOmniboxPopupHost;
}

// Counts popup resources for `profile`, across all browser windows. With
// kTopChromeWebUISharedRenderer, the popups are in TopChromeWebUIProfile, so
// they are counted by their user profile.
PopupResources CountPopupResources(Profile* profile) {
  PopupResources resources;
  for (content::WebContents* contents : content::GetAllWebContents()) {
    if (webui::GetUserProfile(contents) != profile ||
        !IsOmniboxPopup(contents)) {
      continue;
    }
    resources.web_contents.push_back(contents);
    resources.processes.insert(contents->GetPrimaryMainFrame()->GetProcess());
  }
  // Ash reparents popup widgets into a container, so walking the browser's
  // child widgets misses them; ask the presenters instead.
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    if (browser->GetProfile() != profile) {
      continue;
    }
    LocationBarView* location_bar = GetLocationBarView(browser);
    if (!location_bar) {
      continue;
    }
    const OmniboxPopupPresenterBase* presenters[] = {
        location_bar->GetOmniboxPopupPresenterForTesting(),
        location_bar->GetOmniboxPopupAimPresenter()};
    for (const OmniboxPopupPresenterBase* presenter : presenters) {
      // Widgets close asynchronously after hide; ignore ones on their way out.
      const views::Widget* widget =
          presenter ? presenter->GetWidget() : nullptr;
      if (!widget || widget->IsClosed()) {
        continue;
      }
      ++resources.widgets;
      if (!presenter->IsShown()) {
        ++resources.hidden_widgets;
      }
    }
  }
  return resources;
}

// Logs popup counts and private memory footprint, in a grep-friendly format.
void LogPopupResources(std::string_view label,
                       size_t windows,
                       const PopupResources& resources) {
  base::test::TestFuture<
      memory_instrumentation::mojom::RequestOutcome,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump>>
      future;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(base::kNullProcessId,
                                      future.GetCallback());
  auto [outcome, dump] = future.Take();

  std::set<base::ProcessId> popup_pids;
  for (content::RenderProcessHost* process : resources.processes) {
    popup_pids.insert(process->GetProcess().Pid());
  }
  uint64_t popup_process_kb = 0;
  uint64_t renderer_kb = 0;
  uint64_t total_kb = 0;
  if (outcome == memory_instrumentation::mojom::RequestOutcome::kSuccess &&
      dump) {
    for (const auto& process_dump : dump->process_dumps()) {
      const uint64_t kb = process_dump.os_dump().private_footprint_kb;
      total_kb += kb;
      if (process_dump.process_type() ==
          memory_instrumentation::mojom::ProcessType::RENDERER) {
        renderer_kb += kb;
      }
      if (popup_pids.contains(process_dump.pid())) {
        popup_process_kb += kb;
      }
    }
  } else {
    LOG(WARNING) << "OmniboxPopupBudget: memory dump failed";
  }

  LOG(INFO) << "OmniboxPopupBudget[" << label << "] windows=" << windows
            << " web_contents=" << resources.web_contents.size()
            << " widgets=" << resources.widgets
            << " hidden_widgets=" << resources.hidden_widgets
            << " processes=" << resources.processes.size()
            << " popup_process_pmf_kb=" << popup_process_kb
            << " renderer_pmf_kb=" << renderer_kb
            << " total_pmf_kb=" << total_kb;
}

// Returns the popup presenters of `profile`'s browser windows that exist.
std::vector<OmniboxPopupPresenterBase*> GetPopupPresenters(Profile* profile) {
  std::vector<OmniboxPopupPresenterBase*> presenters;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    LocationBarView* location_bar = browser->GetProfile() == profile
                                        ? GetLocationBarView(browser)
                                        : nullptr;
    if (!location_bar) {
      continue;
    }
    OmniboxPopupPresenterBase* window_presenters[] = {
        location_bar->GetOmniboxPopupPresenterForTesting(),
        location_bar->GetOmniboxPopupAimPresenter()};
    for (OmniboxPopupPresenterBase* presenter : window_presenters) {
      if (presenter) {
        presenters.push_back(presenter);
      }
    }
  }
  return presenters;
}

// Waits until every popup presenter has created its content. The deferrals
// (`omnibox::kOmniboxWebUIPopupDeferCreation` and
// `omnibox::kOmniboxAimPopupDeferCreation`, on by default on ChromeOS) create
// it after the window's first presented frame, and the budget is the state
// after that. With `omnibox::kOmniboxWebUIPopupShareAcrossWindows`, a window
// skips creating a popup of a kind that another window has, so it waits until
// one presenter of each kind has its content.
void WaitForPopupContentsToBeCreated(Profile* profile) {
  const bool shared = base::FeatureList::IsEnabled(
      omnibox::kOmniboxWebUIPopupShareAcrossWindows);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    std::vector<OmniboxPopupPresenterBase*> presenters =
        GetPopupPresenters(profile);
    auto kind_has_content = [&](const OmniboxPopupPresenterBase* presenter) {
      return std::ranges::any_of(presenters,
                                 [&](const OmniboxPopupPresenterBase* other) {
                                   return other->GetSharedPopupKind() ==
                                              presenter->GetSharedPopupKind() &&
                                          other->GetWebUIContent();
                                 });
    };
    return std::ranges::all_of(presenters,
                               [&](const OmniboxPopupPresenterBase* presenter) {
                                 return presenter->GetWebUIContent() ||
                                        (shared && kind_has_content(presenter));
                               });
  }));
}

void WaitForPopupsToLoad(Profile* profile) {
  WaitForPopupContentsToBeCreated(profile);
  for (content::WebContents* contents :
       CountPopupResources(profile).web_contents) {
    EXPECT_TRUE(content::WaitForLoadStop(contents));
  }
}

void ExpectPopupLoadsOnlyApp(
    content::WebContents* contents,
    std::string_view expected_app,
    base::span<const std::string_view> forbidden_apps) {
  ASSERT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_EQ(true, content::EvalJs(
                      contents, content::JsReplace(
                                    "customElements.whenDefined($1).then(() => "
                                    "!!document.querySelector($1).shadowRoot)",
                                    expected_app)));
  for (std::string_view forbidden_app : forbidden_apps) {
    EXPECT_EQ(false, content::EvalJs(contents, content::JsReplace(
                                                   "!!customElements.get($1)",
                                                   forbidden_app)))
        << forbidden_app << " should not be registered in " << expected_app;
  }
}

}  // namespace

// Parameterized on whether the full WebUI omnibox popup replaces the classic
// one; both configurations also create the AIM popup.
class OmniboxPopupResourceBudgetBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  // With `measure_production`, measures the popups that production creates
  // for each window, rather than the on-demand creation that
  // `InProcessBrowserTest` sets up.
  explicit OmniboxPopupResourceBudgetBrowserTest(bool aim_eligible = true,
                                                 bool measure_production = true)
      : aim_eligible_(aim_eligible) {
    if (measure_production) {
      create_popup_webui_on_demand_.emplace(
          omnibox::SetCreatePopupWebUIOnDemandForTesting(false));
    }
    std::vector<base::test::FeatureRef> enabled = {
        omnibox::internal::kWebUIOmniboxPopup,
        omnibox::internal::kWebUIOmniboxAimPopup};
    std::vector<base::test::FeatureRef> disabled = {
        features::kWebUILocationBar};
    (IsFullWebUI() ? enabled : disabled)
        .push_back(omnibox::internal::kWebUIOmniboxFullPopup);
    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &OmniboxPopupResourceBudgetBrowserTest::OnCreateServices,
                base::Unretained(this)));
  }

 protected:
  bool IsFullWebUI() const { return GetParam(); }

  // Simulates the eligibility service learning that the profile is eligible,
  // as happens on first run or after sign-in.
  void BecomeAimEligible() {
    aim_eligible_ = true;
    eligibility_changed_callbacks_.Notify();
  }

  // Waits for `profile`'s popups to load, logs them and checks the budgets.
  void CheckProfileBudget(std::string_view label,
                          Profile* profile,
                          size_t windows,
                          size_t max_web_contents) {
    WaitForPopupsToLoad(profile);
    PopupResources resources = CountPopupResources(profile);
    LogPopupResources(label, windows, resources);
    EXPECT_LE(resources.web_contents.size(), max_web_contents);
    EXPECT_LE(resources.hidden_widgets, kMaxHiddenPopupWidgets);
    EXPECT_LE(resources.processes.size(), kPopupRenderProcessesPerProfile);
  }

  void CheckBudget(std::string_view label,
                   Profile* profile,
                   size_t windows,
                   size_t web_contents_per_window) {
    CheckProfileBudget(label, profile, windows,
                       windows * web_contents_per_window);
  }

 private:
  void OnCreateServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &OmniboxPopupResourceBudgetBrowserTest::BuildAimService,
                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildAimService(
      content::BrowserContext* context) {
    auto service =
        std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
            *Profile::FromBrowserContext(context)->GetPrefs(),
            /*template_url_service=*/nullptr,
            /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
            AimEligibilityService::Configuration{});
    // Read the fixture's state at call time so tests can flip eligibility.
    auto eligible = [this] { return aim_eligible_; };
    ON_CALL(*service, IsAimEligible()).WillByDefault(eligible);
    ON_CALL(*service, IsFuseboxEligible()).WillByDefault(eligible);
    ON_CALL(*service, GetLocaleImpl()).WillByDefault(testing::Return("en-US"));
    ON_CALL(*service, RegisterEligibilityChangedCallback(testing::_))
        .WillByDefault([this](base::RepeatingClosure callback) {
          return eligibility_changed_callbacks_.Add(std::move(callback));
        });
    return service;
  }

  bool aim_eligible_;
  base::RepeatingClosureList eligibility_changed_callbacks_;
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::optional<base::AutoReset<bool>> create_popup_webui_on_demand_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxPopupResourceBudgetBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "FullWebUI" : "Classic";
                         });

IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetBrowserTest,
                       ScalesWithWindowCount) {
  Profile* profile = browser()->GetProfile();
  CheckBudget("one_window", profile, 1, kPopupWebContentsPerWindow);

  for (size_t i = 0; i < kExtraWindows; ++i) {
    CreateBrowser(profile);
  }
  CheckBudget("many_windows", profile, 1 + kExtraWindows,
              kPopupWebContentsPerWindow);
}

IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetBrowserTest, IncognitoWindow) {
  BrowserWindowInterface* incognito = CreateIncognitoBrowser();
  CheckBudget("incognito", incognito->GetProfile(), 1,
              kPopupWebContentsPerWindow);
}

// Popup-type windows use the Views popup and must not create WebUI popups.
IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetBrowserTest, PopupWindow) {
  Profile* profile = browser()->GetProfile();
  WaitForPopupsToLoad(profile);
  const size_t before = CountPopupResources(profile).web_contents.size();

  CreateBrowserForPopup(profile);
  PopupResources resources = CountPopupResources(profile);
  LogPopupResources("popup_window", 2, resources);
  EXPECT_EQ(before, resources.web_contents.size());
}

// The classic popup is preloaded for every browser window, so its entry point
// must register its app without pulling in the AI Mode and full popup apps,
// which it never renders.
IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetBrowserTest,
                       ClassicPopupLoadsOnlyClassicApp) {
  if (IsFullWebUI()) {
    GTEST_SKIP() << "Only the classic popup loads omnibox_popup.html.";
  }
  LocationBarView* location_bar = GetLocationBarView(browser());
  ASSERT_TRUE(location_bar);
  WaitForPopupContentsToBeCreated(browser()->GetProfile());
  content::WebContents* contents = location_bar->GetOmniboxPopupView()
                                       ->popup_host()
                                       ->GetWebUIContent()
                                       ->GetWrappedWebContents();
  ExpectPopupLoadsOnlyApp(contents, "omnibox-popup-app",
                          {"omnibox-aim-app", "omnibox-full-app"});
}

// The full popup is preloaded for every browser window, so its entry point
// must register its app without pulling in the classic and AI Mode popup apps,
// which it never renders.
IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetBrowserTest,
                       FullPopupLoadsOnlyFullApp) {
  if (!IsFullWebUI()) {
    GTEST_SKIP() << "Only the full popup loads omnibox_popup_full.html.";
  }
  LocationBarView* location_bar = GetLocationBarView(browser());
  ASSERT_TRUE(location_bar);
  WaitForPopupContentsToBeCreated(browser()->GetProfile());
  content::WebContents* contents = location_bar->GetOmniboxPopupView()
                                       ->popup_host()
                                       ->GetWebUIContent()
                                       ->GetWrappedWebContents();
  ExpectPopupLoadsOnlyApp(contents, "omnibox-full-app",
                          {"omnibox-popup-app", "omnibox-aim-app"});
}

// The AIM popup is preloaded for every browser window, so its entry point
// must register its app without pulling in the classic and full popup apps,
// which it never renders.
IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetBrowserTest,
                       AimPopupLoadsOnlyAimApp) {
  LocationBarView* location_bar = GetLocationBarView(browser());
  ASSERT_TRUE(location_bar);
  WaitForPopupContentsToBeCreated(browser()->GetProfile());
  content::WebContents* contents = location_bar->GetOmniboxPopupAimPresenter()
                                       ->GetWebUIContent()
                                       ->GetWrappedWebContents();
  ExpectPopupLoadsOnlyApp(contents, "omnibox-aim-app",
                          {"omnibox-popup-app", "omnibox-full-app"});
}

// Each popup must load its WebUI once; a WebContents that is created and then
// replaced is wasted work on every window open.
IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetBrowserTest,
                       KeepsEveryWebContentsCreatedForWindow) {
  Profile* profile = browser()->GetProfile();
  WaitForPopupsToLoad(profile);

  std::vector<base::WeakPtr<content::WebContents>> created;
  base::CallbackListSubscription subscription =
      content::RegisterWebContentsCreationCallback(
          base::BindLambdaForTesting([&](content::WebContents* contents) {
            created.push_back(contents->GetWeakPtr());
          }));
  CreateBrowser(profile);
  WaitForPopupsToLoad(profile);

  EXPECT_EQ(std::ranges::count_if(
                created, [](const auto& contents) { return !contents; }),
            0);
}

class OmniboxPopupResourceBudgetAimIneligibleBrowserTest
    : public OmniboxPopupResourceBudgetBrowserTest {
 public:
  OmniboxPopupResourceBudgetAimIneligibleBrowserTest()
      : OmniboxPopupResourceBudgetBrowserTest(/*aim_eligible=*/false) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxPopupResourceBudgetAimIneligibleBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "FullWebUI" : "Classic";
                         });

IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetAimIneligibleBrowserTest,
                       AimIneligibleProfile) {
  CheckBudget("aim_ineligible", browser()->GetProfile(), 1,
              kPopupWebContentsPerAimIneligibleWindow);
}

// Eligibility can arrive after the window exists; the AIM popup must then be
// created so it is available on the next show.
IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetAimIneligibleBrowserTest,
                       CreatesAimPopupOnceEligible) {
  LocationBarView* location_bar = GetLocationBarView(browser());
  ASSERT_TRUE(location_bar);
  EXPECT_FALSE(location_bar->GetOmniboxPopupAimPresenter());

  BecomeAimEligible();
  EXPECT_TRUE(location_bar->GetOmniboxPopupAimPresenter());
  CheckBudget("aim_eligible_later", browser()->GetProfile(), 1,
              kPopupWebContentsPerWindow);
}

// Browser tests create the hidden popups only when they are needed (see
// `omnibox::ShouldCreatePopupWebUIOnDemand()`, which `InProcessBrowserTest`
// turns on). Creating them for every window made browser tests much slower
// and caused shard timeouts (crbug.com/542347163). Fails if a change makes the
// popups load at startup in browser tests again.
//
// Only the classic popup: the full popup's presenter still creates its content
// eagerly.
class OmniboxPopupResourceBudgetInBrowserTestsTest
    : public OmniboxPopupResourceBudgetBrowserTest {
 public:
  // Keeps the browser test default.
  OmniboxPopupResourceBudgetInBrowserTestsTest()
      : OmniboxPopupResourceBudgetBrowserTest(/*aim_eligible=*/true,
                                              /*measure_production=*/false) {}

 protected:
  // Paints `browser`'s window and waits until it presents a frame, after which
  // a deferred popup would create its content.
  [[nodiscard]] bool PaintAndWaitForPresentation(
      BrowserWindowInterface* browser) {
    views::Widget* widget =
        BrowserView::GetBrowserViewForBrowser(browser)->GetWidget();
    base::test::TestFuture<void> presented;
    widget->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
        base::IgnoreArgs<const viz::FrameTimingDetails&>(
            presented.GetCallback()));
    widget->GetRootView()->SchedulePaint();
    if (!presented.Wait()) {
      return false;
    }
    // Run the tasks posted so far, such as a deferred popup's creation task.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    return true;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxPopupResourceBudgetInBrowserTestsTest,
                         testing::Values(false),
                         [](const testing::TestParamInfo<bool>& info) {
                           return "Classic";
                         });

IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetInBrowserTestsTest,
                       NoPopupWebContentsAtStartup) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOmniboxPopupOnDemandCreation)) {
    GTEST_SKIP() << "On-demand creation is turned off by --"
                 << switches::kDisableOmniboxPopupOnDemandCreation;
  }
  Profile* profile = browser()->GetProfile();
  ASSERT_TRUE(PaintAndWaitForPresentation(browser()));
  for (size_t i = 0; i < kExtraWindows; ++i) {
    ASSERT_TRUE(PaintAndWaitForPresentation(CreateBrowser(profile)));
  }
  PopupResources resources = CountPopupResources(profile);
  LogPopupResources("browser_tests_startup", 1 + kExtraWindows, resources);
  EXPECT_EQ(resources.web_contents.size(), 0u);
  EXPECT_EQ(resources.widgets, 0u);

  // The popups still create their content when it is needed.
  LocationBarView* location_bar = GetLocationBarView(browser());
  ASSERT_TRUE(location_bar);
  OmniboxPopupPresenterBase* presenter =
      location_bar->GetOmniboxPopupPresenterForTesting();
  ASSERT_TRUE(presenter);
  EXPECT_FALSE(presenter->GetWebUIContent());
  EXPECT_TRUE(presenter->GetOrCreateWebUIContent(
      OmniboxPopupPresenterBase::CreationReason::kShow));
  EXPECT_EQ(CountPopupResources(profile).web_contents.size(), 1u);
}

// With `kOmniboxWebUIPopupShareAcrossWindows`, a profile's windows share one
// classic and one AI Mode popup, so total popup WebContents stay bounded by
// `kPopupWebContentsPerWindow` regardless of how many windows are open.
//
// Only the classic popup: the full popup is not shared yet.
class OmniboxPopupResourceBudgetShareAcrossWindowsBrowserTest
    : public OmniboxPopupResourceBudgetBrowserTest {
 public:
  OmniboxPopupResourceBudgetShareAcrossWindowsBrowserTest() = default;

 private:
  base::test::ScopedFeatureList share_feature_list_{
      omnibox::kOmniboxWebUIPopupShareAcrossWindows};
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OmniboxPopupResourceBudgetShareAcrossWindowsBrowserTest,
    testing::Values(false),
    [](const testing::TestParamInfo<bool>& info) { return "Classic"; });

IN_PROC_BROWSER_TEST_P(OmniboxPopupResourceBudgetShareAcrossWindowsBrowserTest,
                       DoesNotScaleWithWindowCount) {
  Profile* profile = browser()->GetProfile();
  CheckProfileBudget("shared_one_window", profile, 1,
                     kPopupWebContentsPerWindow);

  BrowserWindowInterface* last_browser = nullptr;
  for (size_t i = 0; i < kExtraWindows; ++i) {
    last_browser = CreateBrowser(profile);
  }
  CheckProfileBudget("shared_many_windows", profile, 1 + kExtraWindows,
                     kPopupWebContentsPerWindow);

  // Using both popups in another window moves the existing WebContents
  // instead of creating new ones.
  LocationBarView* location_bar = GetLocationBarView(last_browser);
  ASSERT_TRUE(location_bar);
  EXPECT_TRUE(location_bar->GetOmniboxPopupPresenterForTesting()
                  ->GetOrCreateWebUIContent(
                      OmniboxPopupPresenterBase::CreationReason::kShow));
  EXPECT_TRUE(
      location_bar->GetOmniboxPopupAimPresenter()->GetOrCreateWebUIContent(
          OmniboxPopupPresenterBase::CreationReason::kShow));
  CheckProfileBudget("shared_after_move", profile, 1 + kExtraWindows,
                     kPopupWebContentsPerWindow);
}
