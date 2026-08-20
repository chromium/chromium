// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_tab_provider_desktop.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/mock_base_window.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

class TestBrowserInstance {
 public:
  explicit TestBrowserInstance(
      Profile* profile,
      BrowserWindowInterface::Type type = BrowserWindowInterface::TYPE_NORMAL,
      bool supports_tab_groups = true)
      : profile_(profile),
        tab_strip_model_(&tab_strip_model_delegate_,
                         profile,
                         supports_tab_groups
                             ? TabGroupModelFactory::GetInstance()
                             : nullptr) {
    tab_strip_model_delegate_.SetBrowserWindowInterface(&mock_bwi_);

    ON_CALL(mock_bwi_, GetProfile()).WillByDefault(testing::Return(profile_));
    ON_CALL(mock_bwi_, GetType()).WillByDefault(testing::Return(type));
    ON_CALL(mock_bwi_, IsDeleteScheduled())
        .WillByDefault(testing::Return(false));
    ON_CALL(mock_bwi_, GetTabStripModel())
        .WillByDefault(testing::Return(&tab_strip_model_));
    ON_CALL(mock_bwi_, GetWindow())
        .WillByDefault(testing::Return(&mock_window_));
    ON_CALL(mock_bwi_, GetFeatures())
        .WillByDefault(testing::ReturnRef(features_));
    ON_CALL(mock_bwi_, RegisterDidBecomeActive(testing::_)).WillByDefault([] {
      return base::CallbackListSubscription();
    });
    ON_CALL(mock_bwi_, RegisterDidBecomeInactive(testing::_)).WillByDefault([] {
      return base::CallbackListSubscription();
    });
    ON_CALL(mock_bwi_, RegisterBrowserDidClose(testing::_))
        .WillByDefault(
            [this](BrowserWindowInterface::BrowserDidCloseCallback cb) {
              return close_callbacks_.Add(std::move(cb));
            });

    BrowserManagerServiceFactory::GetForProfile(profile_)->AddBrowserForTesting(
        &mock_bwi_);
  }

  ~TestBrowserInstance() {
    tab_strip_model_.CloseAllTabs();
    close_callbacks_.Notify(&mock_bwi_);
  }

  TabStripModel* tab_strip_model() { return &tab_strip_model_; }
  BrowserWindowInterface* bwi() { return &mock_bwi_; }

 private:
  raw_ptr<Profile> profile_;
  testing::NiceMock<ui::MockBaseWindow> mock_window_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  TabStripModel tab_strip_model_;
  testing::NiceMock<MockBrowserWindowInterface> mock_bwi_;
  BrowserWindowFeatures features_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)> close_callbacks_;
};

class ContextHubTabProviderDesktopTest : public testing::Test {
 public:
  ContextHubTabProviderDesktopTest() {
    feature_list_.InitWithFeatures({features::kContextHub}, {});
  }

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<tab_groups::FakeTabGroupSyncService>();
        }));
    builder.AddTestingFactory(
        PersonalContextServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<testing::NiceMock<
              personal_context::MockPersonalContextService>>();
        }));
    builder.AddTestingFactory(
        OptimizationGuideKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
    profile_ = builder.Build();

    main_browser_ = std::make_unique<TestBrowserInstance>(profile_.get());
    provider_ = std::make_unique<ContextHubTabProviderDesktop>(profile_.get());
  }

  void TearDown() override {
    provider_.reset();
    main_browser_.reset();
    profile_.reset();
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }
  TestBrowserInstance* browser() { return main_browser_.get(); }

  void AddTab(TestBrowserInstance* browser_instance, const GURL& url) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(
            profile(), content::SiteInstance::Create(profile()));
    sessions::SessionTabHelper::CreateForWebContents(
        contents.get(), sessions::SessionTabHelper::DelegateLookup());
    content::WebContentsTester::For(contents.get())->NavigateAndCommit(url);
    browser_instance->tab_strip_model()->InsertWebContentsAt(
        0, std::move(contents),
        AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER);
  }

  int64_t GetTabId(TestBrowserInstance* target_browser, int index) {
    content::WebContents* wc =
        target_browser->tab_strip_model()->GetWebContentsAt(index);
    return sessions::SessionTabHelper::IdForTab(wc).id();
  }

  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_tab_features_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestBrowserInstance> main_browser_;
  std::unique_ptr<ContextHubTabProviderDesktop> provider_;
};

TEST_F(ContextHubTabProviderDesktopTest, GetTabs_ReturnsAllTabs) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  // Pin one tab and group another tab.
  browser()->tab_strip_model()->SetTabPinned(0, true);
  browser()->tab_strip_model()->AddToNewGroup({1});

  std::vector<content::WebContents*> tabs = provider_->GetTabs();
  EXPECT_EQ(tabs.size(), 3u);
}

TEST_F(ContextHubTabProviderDesktopTest,
       GetUngroupedTabs_ReturnsUngroupedTabsOnly) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  // Group one tab.
  browser()->tab_strip_model()->AddToNewGroup({0});

  std::vector<content::WebContents*> tabs = provider_->GetUngroupedTabs();
  EXPECT_EQ(tabs.size(), 2u);
}

TEST_F(ContextHubTabProviderDesktopTest,
       GetUngroupedTabs_ExcludesPinnedAndUnsupportedWindows) {
  AddTab(browser(), GURL("https://example.com/normal"));
  AddTab(browser(), GURL("https://example.com/pinned"));
  browser()->tab_strip_model()->SetTabPinned(0, true);

  // Window without tab group support (e.g. app window).
  auto app_browser = std::make_unique<TestBrowserInstance>(
      profile(), BrowserWindowInterface::TYPE_APP,
      /*supports_tab_groups=*/false);
  AddTab(app_browser.get(), GURL("https://example.com/app"));

  std::vector<content::WebContents*> tabs = provider_->GetUngroupedTabs();
  ASSERT_EQ(tabs.size(), 1u);
  EXPECT_EQ(tabs[0]->GetVisibleURL(), GURL("https://example.com/normal"));
}

TEST_F(ContextHubTabProviderDesktopTest, ConfirmTabGroups_IgnoresPinnedTabs) {
  AddTab(browser(), GURL("https://example.com/normal"));
  AddTab(browser(), GURL("https://example.com/pinned"));
  browser()->tab_strip_model()->SetTabPinned(0, true);

  int64_t pinned_id = GetTabId(browser(), 0);
  int64_t normal_id = GetTabId(browser(), 1);

  TabGroupEntry group;
  group.label = "Test Group";
  group.tab_ids = {pinned_id, normal_id};

  provider_->ConfirmTabGroups({group});
  EXPECT_FALSE(browser()->tab_strip_model()->GetTabGroupForTab(0).has_value());
  EXPECT_TRUE(browser()->tab_strip_model()->IsTabPinned(0));

  // Assert that the valid unpinned tab was successfully grouped.
  std::optional<tab_groups::TabGroupId> group_id =
      browser()->tab_strip_model()->GetTabGroupForTab(1);
  EXPECT_TRUE(group_id.has_value());
}

TEST_F(ContextHubTabProviderDesktopTest, SwitchToTab_ActivatesTab) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);

  int64_t target_id = GetTabId(browser(), 2);
  provider_->SwitchToTab(target_id);

  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 2);
}

TEST_F(ContextHubTabProviderDesktopTest, CloseTab_ClosesTab) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);

  int64_t target_id = GetTabId(browser(), 1);
  provider_->CloseTab(target_id);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(0)->GetVisibleURL(),
            GURL("https://example.com/3"));
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetVisibleURL(),
            GURL("https://example.com/1"));
}

TEST_F(ContextHubTabProviderDesktopTest, GetTabs_MultipleWindows) {
  AddTab(browser(), GURL("https://example.com/w1_t1"));
  AddTab(browser(), GURL("https://example.com/w1_t2"));

  auto second_browser = std::make_unique<TestBrowserInstance>(
      profile(), BrowserWindowInterface::TYPE_NORMAL,
      /*supports_tab_groups=*/true);
  AddTab(second_browser.get(), GURL("https://example.com/w2_t1"));

  std::vector<content::WebContents*> tabs = provider_->GetTabs();
  EXPECT_EQ(tabs.size(), 3u);
}

TEST_F(ContextHubTabProviderDesktopTest, ConfirmTabGroups_SingleWindow) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  int64_t id1 = GetTabId(browser(), 0);
  int64_t id2 = GetTabId(browser(), 1);

  TabGroupEntry group;
  group.label = "Test Group";
  group.tab_ids = {id1, id2};

  bool result = provider_->ConfirmTabGroups({group});
  EXPECT_TRUE(result);

  std::optional<tab_groups::TabGroupId> group_id =
      browser()->tab_strip_model()->GetTabGroupForTab(0);
  ASSERT_TRUE(group_id.has_value());
  EXPECT_EQ(browser()->tab_strip_model()->GetTabGroupForTab(1), group_id);
  EXPECT_FALSE(browser()->tab_strip_model()->GetTabGroupForTab(2).has_value());
}

TEST_F(ContextHubTabProviderDesktopTest, ConfirmTabGroups_CrossWindow) {
  // Window 1 has 1 tab.
  AddTab(browser(), GURL("https://example.com/1"));
  int64_t id1 = GetTabId(browser(), 0);

  // Window 2 has 2 tabs (majority).
  auto second_browser = std::make_unique<TestBrowserInstance>(
      profile(), BrowserWindowInterface::TYPE_NORMAL,
      /*supports_tab_groups=*/true);
  AddTab(second_browser.get(), GURL("https://example.com/2"));
  AddTab(second_browser.get(), GURL("https://example.com/3"));
  int64_t id2 = GetTabId(second_browser.get(), 0);
  int64_t id3 = GetTabId(second_browser.get(), 1);

  TabGroupEntry group;
  group.label = "Consolidated Group";
  group.tab_ids = {id1, id2, id3};

  bool result = provider_->ConfirmTabGroups({group});
  EXPECT_TRUE(result);

  // The majority window (second_browser) should now hold all 3 tabs grouped.
  EXPECT_EQ(second_browser->tab_strip_model()->count(), 3);
  std::optional<tab_groups::TabGroupId> group_id =
      second_browser->tab_strip_model()->GetTabGroupForTab(0);
  ASSERT_TRUE(group_id.has_value());
  EXPECT_EQ(second_browser->tab_strip_model()->GetTabGroupForTab(1), group_id);
  EXPECT_EQ(second_browser->tab_strip_model()->GetTabGroupForTab(2), group_id);
}

TEST_F(ContextHubTabProviderDesktopTest,
       ConfirmTabGroups_EmptyGroupReturnsFalse) {
  TabGroupEntry group;
  group.label = "Empty";
  group.tab_ids = {};

  EXPECT_FALSE(provider_->ConfirmTabGroups({group}));
  EXPECT_FALSE(provider_->ConfirmTabGroups({}));
}

TEST_F(ContextHubTabProviderDesktopTest,
       UngroupGroupFromTabstripIfOpen_UngroupsTabs) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));

  tab_groups::TabGroupId local_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  auto* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(sync_service);
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroup saved_group(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, {},
      /*position=*/std::nullopt, uuid, local_id);
  sync_service->AddGroup(saved_group);

  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  provider_->UngroupGroupFromTabstripIfOpen(uuid);

  // Tabs remain open, but no longer grouped.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_FALSE(browser()->tab_strip_model()->GetTabGroupForTab(0).has_value());
  EXPECT_FALSE(browser()->tab_strip_model()->GetTabGroupForTab(1).has_value());
}

TEST_F(ContextHubTabProviderDesktopTest,
       RemoveGroupFromTabstripIfOpen_ClosesTabs) {
  AddTab(browser(), GURL("https://example.com/1"));
  AddTab(browser(), GURL("https://example.com/2"));
  AddTab(browser(), GURL("https://example.com/3"));

  tab_groups::TabGroupId local_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  auto* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(sync_service);
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroup saved_group(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, {},
      /*position=*/std::nullopt, uuid, local_id);
  sync_service->AddGroup(saved_group);

  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  provider_->RemoveGroupFromTabstripIfOpen(uuid);

  // Group tabs are closed, only the ungrouped tab remains.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

}  // namespace
}  // namespace context_hub
