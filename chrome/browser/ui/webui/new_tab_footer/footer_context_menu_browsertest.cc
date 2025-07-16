// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

namespace {
const auto kCustomizeMenuItem =
    new_tab_footer::FooterContextMenuItem::kCustomizeChrome;
const auto kHideMenuItem = new_tab_footer::FooterContextMenuItem::kHideFooter;

const char kMenuClickHistogram[] = "NewTabPage.Footer.ContextMenuClicked";
}  // namespace

class FooterContextMenuBrowserTest : public InProcessBrowserTest {
 public:
  FooterContextMenuBrowserTest() = default;
  ~FooterContextMenuBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    BrowserWindowInterface* bwi = webui::GetBrowserWindowInterface(
        chrome_test_utils::GetActiveWebContents(this));
    menu_ = std::make_unique<FooterContextMenu>(bwi);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDownOnMainThread() override {
    menu_.reset();
    histogram_tester_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  bool IsCommandIdVisible(new_tab_footer::FooterContextMenuItem menu_item) {
    return menu()->IsCommandIdVisible(static_cast<int>(menu_item));
  }

  void ExecuteCommand(new_tab_footer::FooterContextMenuItem menu_item) {
    menu()->ExecuteCommand(static_cast<int>(menu_item), /*event_flags=*/0);
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }
  FooterContextMenu* menu() { return menu_.get(); }
  Profile* profile() { return browser()->profile(); }

 private:
  std::unique_ptr<FooterContextMenu> menu_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(FooterContextMenuBrowserTest, ShowsMenuItems) {
  EXPECT_TRUE(IsCommandIdVisible(kHideMenuItem));
  EXPECT_TRUE(IsCommandIdVisible(kCustomizeMenuItem));
}

IN_PROC_BROWSER_TEST_F(FooterContextMenuBrowserTest, HidesFooter) {
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible));
  histogram_tester()->ExpectTotalCount(kMenuClickHistogram, 0);

  ExecuteCommand(kHideMenuItem);

  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible));
  histogram_tester()->ExpectTotalCount(kMenuClickHistogram, 1);
  histogram_tester()->ExpectBucketCount(kMenuClickHistogram, kHideMenuItem, 1);
}

IN_PROC_BROWSER_TEST_F(FooterContextMenuBrowserTest, OpensCustomizeChrome) {
  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(kActionSidePanelShowCustomizeChromeFooter)
                ->GetInvokeCount(),
            0);
  histogram_tester()->ExpectTotalCount(kMenuClickHistogram, 0);

  ExecuteCommand(kCustomizeMenuItem);

  ASSERT_EQ(actions::ActionManager::Get()
                .FindAction(kActionSidePanelShowCustomizeChromeFooter)
                ->GetInvokeCount(),
            1);
  histogram_tester()->ExpectTotalCount(kMenuClickHistogram, 1);
  histogram_tester()->ExpectBucketCount(kMenuClickHistogram, kCustomizeMenuItem,
                                        1);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class FooterContextMenuEnterpriseTest : public FooterContextMenuBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kEnterpriseBadgingForNtpFooter);
    FooterContextMenuBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Simulate browser management.
    scoped_browser_management_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(profile()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    FooterContextMenuBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    scoped_browser_management_.reset();
    FooterContextMenuBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FooterContextMenuEnterpriseTest,
  ShowsMenuItemsByDefault) {
  EXPECT_TRUE(IsCommandIdVisible(kHideMenuItem));
  EXPECT_TRUE(IsCommandIdVisible(kCustomizeMenuItem));
}

IN_PROC_BROWSER_TEST_F(FooterContextMenuEnterpriseTest, PolicyDisablesHideMenuOption) {
  g_browser_process->local_state()->SetString(
      prefs::kEnterpriseCustomLabelForBrowser, "Custom label");

  EXPECT_FALSE(IsCommandIdVisible(kHideMenuItem));
  EXPECT_TRUE(IsCommandIdVisible(kCustomizeMenuItem));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
