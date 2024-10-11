// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/json/json_reader.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using testing::_;

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kShouldPreventClose = true;
#else
constexpr bool kShouldPreventClose = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class TabStripModelPreventCloseTest : public PreventCloseTestBase,
                                      public BrowserListObserver,
                                      public TabStripModelObserver {
 public:
  TabStripModelPreventCloseTest() { BrowserList::AddObserver(this); }

  explicit TabStripModelPreventCloseTest(const PreventCloseTestBase&) = delete;
  TabStripModelPreventCloseTest& operator=(
      const TabStripModelPreventCloseTest&) = delete;

  ~TabStripModelPreventCloseTest() override {
    BrowserList::RemoveObserver(this);
  }

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override { observer_.Reset(); }

  // TabStripModelObserver:
  MOCK_METHOD(void,
              TabCloseCancelled,
              (const content::WebContents* contents),
              (override));

 protected:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
  base::ScopedObservation<TabStripModel, TabStripModelPreventCloseTest>
      observer_{this};
};

IN_PROC_BROWSER_TEST_F(TabStripModelPreventCloseTest,
                       PreventCloseEnforedByPolicy) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(web_app::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);
  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  observer_.Observe(browser->tab_strip_model());

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(!kShouldPreventClose, tab_strip_model->IsTabClosable(
                                      tab_strip_model->GetActiveWebContents()));

  EXPECT_CALL(*this, TabCloseCancelled(_)).Times(kShouldPreventClose ? 1 : 0);

  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(kShouldPreventClose ? 1 : 0, tab_strip_model->count());

  if (kShouldPreventClose) {
    ClearWebAppSettings();
    EXPECT_TRUE(tab_strip_model->IsTabClosable(
        tab_strip_model->GetActiveWebContents()));

    tab_strip_model->CloseAllTabs();
    EXPECT_EQ(0, tab_strip_model->count());
  }
}

// TODO(b/321593065): enable this flaky test.
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable \
  DISABLED_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable
#else
#define MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable \
  PreventCloseEnforcedByPolicyTabbedAppShallBeClosable
#endif
IN_PROC_BROWSER_TEST_F(
    TabStripModelPreventCloseTest,
    MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(web_app::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);
  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/false);
  ASSERT_TRUE(browser);

  observer_.Observe(browser->tab_strip_model());

  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  EXPECT_NE(0, tab_strip_model->count());
  EXPECT_TRUE(
      tab_strip_model->IsTabClosable(tab_strip_model->GetActiveWebContents()));

  EXPECT_CALL(*this, TabCloseCancelled(_)).Times(0);

  tab_strip_model->CloseAllTabs();
  EXPECT_EQ(0, tab_strip_model->count());
}

class TabStripModelBrowserTest : public InProcessBrowserTest,
                                 public TabStripModelObserver {
 public:
  TabStripModelBrowserTest() {
    feature_list_.InitWithFeatures({features::kTabOrganization}, {});
  }

  void TearDownOnMainThread() override { observer_.Reset(); }

  MOCK_METHOD(void,
              OnTabGroupAdded,
              (const tab_groups::TabGroupId& group_id),
              (override));
  MOCK_METHOD(void,
              OnTabGroupWillBeRemoved,
              (const tab_groups::TabGroupId& group_id),
              (override));

  base::test::ScopedFeatureList feature_list_;
  base::ScopedObservation<TabStripModel, TabStripModelBrowserTest> observer_{
      this};
};

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, OnTabGroupAdded) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // We should already have a tab. Add it to a group and see if
  // TabStripModelObserver::OnTabGroupAdded is called.
  EXPECT_CALL(*this, OnTabGroupAdded(_)).Times(1);

  observer_.Observe(browser()->tab_strip_model());
  browser()->tab_strip_model()->AddToNewGroup({0});
}

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, OnTabGroupWillBeRemoved) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  // Close the group and see if TabStripModelObserver::OnTabGroupWillBeRemoved
  // is called.
  EXPECT_CALL(*this, OnTabGroupWillBeRemoved(group_id)).Times(1);

  observer_.Observe(browser()->tab_strip_model());
  browser()->tab_strip_model()->CloseAllTabsInGroup(group_id);
}

IN_PROC_BROWSER_TEST_F(TabStripModelBrowserTest, CommandOrganizeTabs) {
  base::HistogramTester histogram_tester;

  TabStripModel* const tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());

  EXPECT_TRUE(tab_strip_model->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandOrganizeTabs));

  // Execute CommandOrganizeTabs once. Expect a request to have been started.
  tab_strip_model->ExecuteContextMenuCommand(
      0, TabStripModel::CommandOrganizeTabs);

  TabOrganizationService* const service =
      TabOrganizationServiceFactory::GetForProfile(browser()->profile());
  const TabOrganizationSession* const session =
      service->GetSessionForBrowser(browser());
  EXPECT_NE(session, nullptr);
  EXPECT_EQ(session->request()->state(),
            TabOrganizationRequest::State::NOT_STARTED);

  histogram_tester.ExpectUniqueSample("Tab.Organization.AllEntrypoints.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.TabContextMenu.Clicked",
                                      true, 1);
}
