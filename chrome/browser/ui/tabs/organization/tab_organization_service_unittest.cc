// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_observer.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "tab_organization_service_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace {

constexpr char kValidURL[] = "http://zombo.com";
constexpr char kInvalidURL[] = "chrome://page";

std::unique_ptr<KeyedService> CreateSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // anonymous namespace

class TabOrganizationServiceTest : public BrowserWithTestWindowTest {
 public:
  TabOrganizationServiceTest()
      : dependency_manager_subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &TabOrganizationServiceTest::SetTestingFactories,
                    base::Unretained(this)))) {}
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

  GURL GetUniqueTestURL() {
    static int offset = 1;
    GURL url("http://page_" + base::NumberToString(offset));
    offset++;
    return url;
  }

  content::WebContents* AddValidTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    content::WebContentsTester::For(web_contents.get())
        ->NavigateAndCommit(GURL(kValidURL));

    content::WebContents* web_contents_ptr = web_contents.get();
    content::WebContentsTester::For(web_contents_ptr)
        ->NavigateAndCommit(GURL(GetUniqueTestURL()));

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

    return web_contents_ptr;
  }

  TestingProfile* profile() { return profile_.get(); }
  TabOrganizationService* service() { return service_.get(); }
  syncer::TestSyncService* sync_service() { return sync_service_; }

 private:
  void SetUp() override {
    feature_list_.InitWithFeatures({features::kTabOrganization}, {});
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<TabOrganizationService>(profile_.get());
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->GetForProfile(profile_.get()));
  }
  void TearDown() override {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  void SetTestingFactories(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateSyncService));
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TabOrganizationService> service_;
  std::vector<std::unique_ptr<Browser>> browsers_;
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<syncer::TestSyncService> sync_service_;
  base::CallbackListSubscription dependency_manager_subscription_;
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

TEST_F(TabOrganizationServiceTest, DoesntAddSessionOnTriggerIfExists) {
  Browser* browser = AddBrowser();
  AddValidTabToBrowser(browser, 0);
  service()->OnTriggerOccured(browser);
  service()->CreateSessionForBrowser(browser);
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
  service()->CreateSessionForBrowser(browser1);
  service()->CreateSessionForBrowser(browser2);
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
    AddValidTabToBrowser(browser1, 0);
  }
  std::unique_ptr<TabOrganizationSession> session =
      TabOrganizationSession::CreateSessionForBrowser(browser1);
  EXPECT_EQ(session->request()->tab_datas().size(), 4u);

  session->StartRequest();
  EXPECT_NE(session->request()->response(), nullptr);
  EXPECT_EQ(session->tab_organizations().size(), 1u);

  TabOrganization* organization = session->GetNextTabOrganization();
  ASSERT_TRUE(organization);

  organization->Accept();
  EXPECT_EQ(browser1->tab_strip_model()->group_model()->ListTabGroups().size(),
            1u);
}

TEST_F(TabOrganizationServiceTest,
       TabOrganizationSessionCreateSessionForBrowserNoInvalidTabDatas) {
  const int valid_tab_count = 2;
  Browser* browser1 = AddBrowser();
  for (int i = 0; i < valid_tab_count; i++) {
    AddValidTabToBrowser(browser1, 0);
  }

  // Add an invalid tab.
  content::WebContents* invalid_web_contents = AddValidTabToBrowser(browser1, 0);
  content::WebContentsTester::For(invalid_web_contents)
      ->NavigateAndCommit(GURL(kInvalidURL));

  std::unique_ptr<TabOrganizationSession> session =
      TabOrganizationSession::CreateSessionForBrowser(browser1);
  EXPECT_EQ(static_cast<int>(session->request()->tab_datas().size()),
            valid_tab_count);

  session->StartRequest();
  EXPECT_NE(session->request()->response(), nullptr);
  EXPECT_EQ(session->tab_organizations().size(), 1u);

  TabOrganization* organization = session->GetNextTabOrganization();
  EXPECT_TRUE(organization);

  organization->Accept();
  EXPECT_EQ(browser1->tab_strip_model()->group_model()->ListTabGroups().size(),
            1u);
}

TEST_F(TabOrganizationServiceTest,
       TabOrganizationSessionResetSessionForBrowser) {
  const int valid_tab_count = 2;
  Browser* browser1 = AddBrowser();
  for (int i = 0; i < valid_tab_count; i++) {
    AddValidTabToBrowser(browser1, 0);
  }

  TabOrganizationSession::ID session_id_1 =
      service()->CreateSessionForBrowser(browser1)->session_id();
  TabOrganizationSession::ID session_id_2 =
      service()->ResetSessionForBrowser(browser1)->session_id();
  EXPECT_NE(session_id_1, session_id_2);
}

TEST_F(TabOrganizationServiceTest, SecondRequestAfterCompletionDoesntCrash) {
  Browser* browser1 = AddBrowser();
  for (int i = 0; i < 4; i++) {
    AddValidTabToBrowser(browser1, 0);
  }

  service()->StartRequest(browser1);
  auto* const session = service()->GetSessionForBrowser(browser1);
  ASSERT_EQ(session->tab_organizations().size(), 1u);
  session->GetNextTabOrganization()->Accept();
  ASSERT_TRUE(session->IsComplete());

  service()->StartRequest(browser1);
}

TEST_F(TabOrganizationServiceTest, SecondRequestAfterStartingDoesntCrash) {
  Browser* browser1 = AddBrowser();
  for (int i = 0; i < 4; i++) {
    AddValidTabToBrowser(browser1, 0);
  }

  service()->StartRequest(browser1);
  auto* const session = service()->GetSessionForBrowser(browser1);
  ASSERT_EQ(session->tab_organizations().size(), 1u);
  ASSERT_FALSE(session->IsComplete());

  service()->StartRequest(browser1);
}

// Session Creation Tests
TEST_F(TabOrganizationServiceTest, CreateSessionForBrowserOnTab) {
  Browser* browser1 = AddBrowser();

  content::WebContents* base_tab = AddValidTabToBrowser(browser1, 0);
  for (int i = 0; i < 4; i++) {
    AddValidTabToBrowser(browser1, 0);
  }

  std::unique_ptr<TabOrganizationSession> session =
      TabOrganizationSession::CreateSessionForBrowser(browser1, base_tab);
  EXPECT_NE(session->request()->base_tab_id(), absl::nullopt);
}

TEST_F(TabOrganizationServiceTest, CanStartRequest) {
  // // Not Synced
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN});
  EXPECT_FALSE(service()->CanStartRequest());
  sync_service()->SetDisableReasons({});

  // Sync Paused
  sync_service()->SetPersistentAuthError();
  EXPECT_FALSE(service()->CanStartRequest());
  sync_service()->ClearAuthError();

  // Sync History not enabled
  ASSERT_TRUE(sync_service()->GetActiveDataTypes().HasAll({syncer::HISTORY}));
  sync_service()->GetUserSettings()->SetSelectedTypes(false, {});
  ASSERT_FALSE(sync_service()->GetActiveDataTypes().HasAll({syncer::HISTORY}));
  EXPECT_FALSE(service()->CanStartRequest());

  sync_service()->GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kHistory});
  EXPECT_TRUE(service()->CanStartRequest());

  // Should return true if everything is enabled.
  sync_service()->GetUserSettings()->SetSelectedTypes(true, {});
  EXPECT_TRUE(service()->CanStartRequest());
}

TEST_F(TabOrganizationServiceTest, EnterpriseDisabledPolicy) {
  EXPECT_TRUE(service()->CanStartRequest());
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});
  EXPECT_FALSE(service()->CanStartRequest());
}

TEST_F(TabOrganizationServiceTest, TabStripAddRemoveDestroysSession) {
  Browser* browser1 = AddBrowser();
  for (int i = 0; i < 4; i++) {
    AddValidTabToBrowser(browser1, 0);
  }

  service()->CreateSessionForBrowser(browser1);
  content::WebContents* contents = AddValidTabToBrowser(browser1, 0);
  EXPECT_EQ(service()->GetSessionForBrowser(browser1), nullptr);

  service()->CreateSessionForBrowser(browser1);
  browser1->tab_strip_model()->CloseWebContentsAt(
      browser1->tab_strip_model()->GetIndexOfWebContents(contents),
      TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(service()->GetSessionForBrowser(browser1), nullptr);
}

TEST_F(TabOrganizationServiceTest,
       RemoveAllTabsWhileMultiplePendingOrganizationsDoesntCrash) {
  // b/319272034

  // This is a regression test for a crash when:
  // - there are at least two pending organizations in the current session
  // - the UI is open (and so TabSearchPageHandler is observing the session)
  // - the browser is closed using TabStripModel::CloseAllTabs
  // - probably some other requirements on observer registration order

  // Then the page handler calls IsValidForOrganizing() on one of the
  // organizations, which crashed because the tabs in the organization no longer
  // existed BUT it had not yet been notified of this.

  // This observer simulates the role of the TabSearchPageHandler
  class TestOrganizationObserver : public TabOrganization::Observer {
   public:
    explicit TestOrganizationObserver(TabOrganization* org_1,
                                      TabOrganization* org_2)
        : org_1_(org_1), org_2_(org_2) {}
    void OnTabOrganizationUpdated(const TabOrganization* org) override {
      // Without the fix, one of these two lines will crash, because only one of
      // the orgs has been notified of the removed tabs, and the other assumes
      // its tabs are still alive.
      org_1_->IsValidForOrganizing();
      org_2_->IsValidForOrganizing();
    }

   private:
    raw_ptr<TabOrganization> org_1_;
    raw_ptr<TabOrganization> org_2_;
  };

  Browser* browser1 = AddBrowser();
  for (int i = 0; i < 4; i++) {
    AddValidTabToBrowser(browser1, 0);
  }

  TabStripModel* model = browser1->tab_strip_model();
  std::vector<std::u16string> names;

  // Create two organizations. This must be done manually instead of through the
  // service and session because the test scaffold using the service doesn't let
  // us match the observer registration order that actually triggers the crash.
  // The contents of the organizations doesn't matter.
  std::vector<std::unique_ptr<TabData>> tab_datas_1;
  tab_datas_1.emplace_back(
      std::make_unique<TabData>(model, model->GetWebContentsAt(0)));
  tab_datas_1.emplace_back(
      std::make_unique<TabData>(model, model->GetWebContentsAt(1)));
  TabOrganization org_1 =
      TabOrganization(std::move(tab_datas_1), names, 0u,
                      TabOrganization::UserChoice::kNoChoice);

  std::vector<std::unique_ptr<TabData>> tab_datas_2;
  tab_datas_2.emplace_back(
      std::make_unique<TabData>(model, model->GetWebContentsAt(0)));
  tab_datas_2.emplace_back(
      std::make_unique<TabData>(model, model->GetWebContentsAt(1)));
  TabOrganization org_2 =
      TabOrganization(std::move(tab_datas_2), names, 0u,
                      TabOrganization::UserChoice::kNoChoice);

  TestOrganizationObserver observer(&org_1, &org_2);
  org_1.AddObserver(&observer);
  org_2.AddObserver(&observer);

  // This triggers the test observer, and without the fix crashes when only one
  // of the organizations has been updated with the closed tabs but
  // IsValidForOrganizing is called on the other one.
  browser1->tab_strip_model()->CloseAllTabs();

  // Remove the observer so it is memory-safe to destroy it first.
  org_1.RemoveObserver(&observer);
  org_2.RemoveObserver(&observer);
}
