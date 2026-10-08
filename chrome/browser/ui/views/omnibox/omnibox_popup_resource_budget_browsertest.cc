// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Budgets for the resources WebUI omnibox popups consume per browser window.
// These guard against regressions in popup WebContents, widget and renderer
// process counts; tighten them as optimizations land. Memory is logged, not
// asserted, since it is too noisy to gate on. Run with
// --test-launcher-print-test-stdio=always to see the "OmniboxPopupBudget"
// lines.

#include <memory>
#include <set>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

// Classic (or full) popup plus the AIM popup, created eagerly per window.
constexpr size_t kPopupWebContentsPerWindow = 2;
// Widgets are created on first show, so only a popup that is showing (at most
// the focused window's, e.g. the full popup on startup) has one.
constexpr size_t kMaxPopupWidgets = 1;
// AIM-ineligible profiles get only the classic (or full) popup.
constexpr size_t kPopupWebContentsPerAimIneligibleWindow = 1;
// Top-chrome WebUIs of one profile share a renderer process.
constexpr size_t kPopupRenderProcessesPerProfile = 1;

constexpr size_t kExtraWindows = 4;

struct PopupResources {
  std::vector<raw_ptr<content::WebContents>> web_contents;
  std::set<raw_ptr<content::RenderProcessHost>> processes;
  size_t widgets = 0;
};

LocationBarView* GetLocationBarView(BrowserWindowInterface* browser) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
  return toolbar ? toolbar->location_bar_view() : nullptr;
}

bool IsOmniboxPopup(content::WebContents* contents) {
  return contents->GetVisibleURL().host() == chrome::kChromeUIOmniboxPopupHost;
}

// Counts popup resources owned by `profile`, across all browser windows.
PopupResources CountPopupResources(Profile* profile) {
  PopupResources resources;
  for (content::WebContents* contents : content::GetAllWebContents()) {
    if (contents->GetBrowserContext() != profile || !IsOmniboxPopup(contents)) {
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
        location_bar->GetOmniboxPopupView()->presenter(),
        location_bar->GetOmniboxPopupAimPresenter()};
    for (const OmniboxPopupPresenterBase* presenter : presenters) {
      if (presenter && presenter->GetWidget()) {
        ++resources.widgets;
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
            << " processes=" << resources.processes.size()
            << " popup_process_pmf_kb=" << popup_process_kb
            << " renderer_pmf_kb=" << renderer_kb
            << " total_pmf_kb=" << total_kb;
}

void WaitForPopupsToLoad(Profile* profile) {
  for (content::WebContents* contents :
       CountPopupResources(profile).web_contents) {
    EXPECT_TRUE(content::WaitForLoadStop(contents));
  }
}

}  // namespace

// Parameterized on whether the full WebUI omnibox popup replaces the classic
// one; both configurations also create the AIM popup.
class OmniboxPopupResourceBudgetBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  explicit OmniboxPopupResourceBudgetBrowserTest(bool aim_eligible = true)
      : aim_eligible_(aim_eligible) {
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
  void CheckBudget(std::string_view label,
                   Profile* profile,
                   size_t windows,
                   size_t web_contents_per_window) {
    WaitForPopupsToLoad(profile);
    PopupResources resources = CountPopupResources(profile);
    LogPopupResources(label, windows, resources);
    EXPECT_LE(resources.web_contents.size(), windows * web_contents_per_window);
    EXPECT_LE(resources.widgets, kMaxPopupWidgets);
    EXPECT_LE(resources.processes.size(), kPopupRenderProcessesPerProfile);
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
