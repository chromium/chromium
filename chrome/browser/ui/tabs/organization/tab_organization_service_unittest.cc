// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "tab_organization_service_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

class TabOrganizationServiceTest : public BrowserWithTestWindowTest {
 public:
  TabOrganizationServiceTest() = default;
  TabOrganizationServiceTest(const TabOrganizationServiceTest&) = delete;
  TabOrganizationServiceTest& operator=(const TabOrganizationServiceTest&) =
      delete;

  Browser* AddBrowser() {
    Browser::CreateParams native_params(profile_.get(), true);
    native_params.initial_show_state = ui::SHOW_STATE_DEFAULT;
    std::unique_ptr<Browser> browser =
        CreateBrowserWithTestWindowForParams(native_params);
    Browser* browser_ptr = browser.get();
    browsers_.emplace_back(std::move(browser));
    return browser_ptr;
  }

  content::WebContents* AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);

    content::WebContents* web_contents_ptr = web_contents.get();

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

    return web_contents_ptr;
  }

  TestingProfile* profile() { return profile_.get(); }
  TabOrganizationService* service() { return service_.get(); }

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<TabOrganizationService>(profile_.get());
  }
  void TearDown() override {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TabOrganizationService> service_;
  std::vector<std::unique_ptr<Browser>> browsers_;
};

class MockTabOrganizationObserver : public TabOrganizationObserver {
 public:
  MOCK_METHOD(void, OnToggleActionUIState, (const Browser*, bool), (override));
};

// Service Factory tests.

TEST_F(TabOrganizationServiceTest, DifferentSessionPerProfile) {
  std::unique_ptr<TestingProfile> profile_1 =
      std::make_unique<TestingProfile>();
  std::unique_ptr<TestingProfile> profile_2 =
      std::make_unique<TestingProfile>();

  TabOrganizationService* service_1 =
      TabOrganizationServiceFactory::GetForProfile(profile_1.get());
  TabOrganizationService* service_2 =
      TabOrganizationServiceFactory::GetForProfile(profile_2.get());

  EXPECT_NE(service_1, service_2);
}

TEST_F(TabOrganizationServiceTest, NoIncognito) {
  TestingProfile::Builder incognito_builder;
  TestingProfile* incognito_profile =
      incognito_builder.BuildIncognito(profile());
  TabOrganizationService* incognito_service =
      TabOrganizationServiceFactory::GetForProfile(incognito_profile);

  EXPECT_EQ(incognito_service, nullptr);
}

// Service tests.

TEST_F(TabOrganizationServiceTest, AddsSessionOnTrigger) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  service()->OnTriggerOccured(browser);
  EXPECT_TRUE(base::Contains(service()->browser_session_map(), browser));
  EXPECT_NE(service()->GetSessionForBrowser(browser), nullptr);
}

TEST_F(TabOrganizationServiceTest, DoesntAddSessionOnTriggerIfExists) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  service()->OnTriggerOccured(browser);
  EXPECT_TRUE(base::Contains(service()->browser_session_map(), browser));
  const TabOrganizationSession* session =
      service()->GetSessionForBrowser(browser);
  EXPECT_NE(session, nullptr);

  service()->OnTriggerOccured(browser);
  EXPECT_TRUE(base::Contains(service()->browser_session_map(), browser));
  EXPECT_EQ(session, service()->GetSessionForBrowser(browser));
}

TEST_F(TabOrganizationServiceTest, EachBrowserHasADistinctSession) {
  Browser* browser1 = AddBrowser();
  Browser* browser2 = AddBrowser();
  service()->OnTriggerOccured(browser1);
  service()->OnTriggerOccured(browser2);
  EXPECT_NE(service()->GetSessionForBrowser(browser1),
            service()->GetSessionForBrowser(browser2));
}

TEST_F(TabOrganizationServiceTest, ObserverShowTriggerUICalled) {
  Browser* browser = AddBrowser();

  MockTabOrganizationObserver mock_observer;
  EXPECT_CALL(mock_observer, OnToggleActionUIState(browser, true)).Times(1);

  service()->AddObserver(&mock_observer);
  service()->OnTriggerOccured(browser);
  service()->RemoveObserver(&mock_observer);
}

TEST_F(TabOrganizationServiceTest, SessionFromBrowserPopulatesRequest) {
  Browser* browser1 = AddBrowser();
  service()->OnTriggerOccured(browser1);
  for (int i = 0; i < 4; i++) {
    AddTabToBrowser(browser1, 0);
  }
  std::unique_ptr<TabOrganizationSession> session =
      TabOrganizationSession::CreateSessionForBrowser(browser1, service());
  EXPECT_EQ(session->request()->tab_datas().size(), 4u);

  session->StartRequest();
  EXPECT_NE(session->request()->response(), nullptr);
  EXPECT_EQ(session->tab_organizations().size(), 1u);
  EXPECT_EQ(browser1->tab_strip_model()->group_model()->ListTabGroups().size(),
            1u);
}
