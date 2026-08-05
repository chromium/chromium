// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace contextual_tasks {
namespace {

using ::testing::_;
using ::testing::Const;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class MockContextualTasksContextService
    : public ContextualTasksContextService {
 public:
  explicit MockContextualTasksContextService(Profile* profile)
      : ContextualTasksContextService(profile) {}
  MOCK_METHOD(void,
              GetRelevantTabsForConversationThread,
              (const TabSelectionOptions&,
               const ConversationThread&,
               const std::vector<GURL>&,
               base::OnceCallback<
                   void(std::vector<base::WeakPtr<content::WebContents>>)>),
              (override));
};

class MockActiveTaskContextProvider : public ActiveTaskContextProvider {
 public:
  MOCK_METHOD(void, AddObserver, (Observer* observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer* observer), (override));
  MOCK_METHOD(void, RefreshContext, (), (override));
  MOCK_METHOD(void,
              SetContextualTasksPanelController,
              (ContextualTasksPanelController*),
              (override));
  MOCK_METHOD(void, AddLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, RemoveLocalTabUnderline, (tabs::TabHandle), (override));
  MOCK_METHOD(void, ClearAllLocalTabUnderlines, (), (override));
};

class DesktopQueryContextualizerDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if !BUILDFLAG(IS_ANDROID)
    InitializeActionIdStringMapping();
#endif

    ContextualSearchServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return nullptr;
        }));
    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return nullptr;
        }));
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return nullptr;
        }));

    browser_window_ = std::make_unique<NiceMock<MockBrowserWindowInterface>>();
    browser_window_weak_ptr_factory_ =
        std::make_unique<base::WeakPtrFactory<BrowserWindowInterface>>(
            browser_window_.get());
    ON_CALL(*browser_window_, GetWeakPtr())
        .WillByDefault(
            Return(browser_window_weak_ptr_factory_->GetWeakPtr()));
    ON_CALL(*browser_window_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(Const(*browser_window_), GetProfile())
        .WillByDefault(Return(profile()));

    ON_CALL(*browser_window_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));
    ON_CALL(Const(*browser_window_), GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(unowned_user_data_host_));

    tab_list_ = std::make_unique<NiceMock<MockTabListInterface>>();
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            unowned_user_data_host_, *tab_list_);
    ON_CALL(*tab_list_, GetActiveTab()).WillByDefault(Return(&mock_active_tab_));

    mock_service_ =
        std::make_unique<NiceMock<MockContextualTasksContextService>>(
            profile());

    std::unique_ptr<ContextualTasksPanelHost> mock_panel_host =
        std::make_unique<NiceMock<MockContextualTasksPanelHost>>();
    mock_panel_host_ =
        static_cast<MockContextualTasksPanelHost*>(mock_panel_host.get());
    ON_CALL(*mock_panel_host_, IsPanelInitialized())
        .WillByDefault(Return(true));

    coordinator_ = std::make_unique<ContextualTasksSidePanelCoordinator>(
        browser_window_.get(), std::move(mock_panel_host),
        &mock_active_task_context_provider_, nullptr);

    web_contents_active_ = CreateTestWebContents();
    web_contents_other_ = CreateTestWebContents();

    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents_active_.get(), &mock_active_tab_);
    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents_other_.get(), &mock_other_tab_);

    ON_CALL(mock_active_tab_, GetContents())
        .WillByDefault(Return(web_contents_active_.get()));
    ON_CALL(mock_active_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(active_tab_user_data_host_));
    ON_CALL(Const(mock_active_tab_), GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(active_tab_user_data_host_));

    ON_CALL(mock_other_tab_, GetContents())
        .WillByDefault(Return(web_contents_other_.get()));
    ON_CALL(mock_other_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(other_tab_user_data_host_));
    ON_CALL(Const(mock_other_tab_), GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(other_tab_user_data_host_));

    ON_CALL(*browser_window_, GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));
    ON_CALL(Const(*browser_window_), GetFeatures())
        .WillByDefault(ReturnRef(browser_window_features_));
    ON_CALL(*browser_window_, GetActiveTabInterface())
        .WillByDefault(Return(&mock_active_tab_));

    delegate_ = std::make_unique<DesktopQueryContextualizerDelegate>(
        base::BindRepeating(
            []() -> contextual_search::ContextualSearchSessionHandle* {
              return nullptr;
            }),
        base::BindRepeating(
            []() -> std::optional<lens::ImageEncodingOptions> {
              return std::nullopt;
            }),
        mock_service_.get(),
        base::BindRepeating(
            &DesktopQueryContextualizerDelegateTest::browser_window,
            base::Unretained(this)));
  }

  void TearDown() override {
    delegate_.reset();
    mock_panel_host_ = nullptr;
    coordinator_.reset();
    tab_list_registration_.reset();
    tab_list_.reset();
    web_contents_active_.reset();
    web_contents_other_.reset();
    mock_service_.reset();
    browser_window_weak_ptr_factory_.reset();
    browser_window_.reset();
#if !BUILDFLAG(IS_ANDROID)
    actions::ActionIdMap::ResetMapsForTesting();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  BrowserWindowInterface* browser_window() { return browser_window_.get(); }

 protected:
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<NiceMock<MockBrowserWindowInterface>> browser_window_;
  std::unique_ptr<base::WeakPtrFactory<BrowserWindowInterface>>
      browser_window_weak_ptr_factory_;
  BrowserWindowFeatures browser_window_features_;
  std::unique_ptr<NiceMock<MockTabListInterface>> tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  NiceMock<MockActiveTaskContextProvider> mock_active_task_context_provider_;
  std::unique_ptr<MockContextualTasksContextService> mock_service_;
  raw_ptr<MockContextualTasksPanelHost> mock_panel_host_;
  std::unique_ptr<ContextualTasksSidePanelCoordinator> coordinator_;

  tabs::MockTabInterface mock_active_tab_;
  tabs::MockTabInterface mock_other_tab_;
  ui::UnownedUserDataHost active_tab_user_data_host_;
  ui::UnownedUserDataHost other_tab_user_data_host_;
  std::unique_ptr<content::WebContents> web_contents_active_;
  std::unique_ptr<content::WebContents> web_contents_other_;

  std::unique_ptr<DesktopQueryContextualizerDelegate> delegate_;
};

TEST_F(DesktopQueryContextualizerDelegateTest,
       GetRelevantTabs_PanelClosed_ReturnsOnlyServiceTabs) {
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(false));

  EXPECT_CALL(*mock_service_,
              GetRelevantTabsForConversationThread(_, _, _, _))
      .WillOnce(
          [this](const TabSelectionOptions& options,
                 const ConversationThread& conversation_thread,
                 const std::vector<GURL>& explicit_urls,
                 base::OnceCallback<void(
                     std::vector<base::WeakPtr<content::WebContents>>)> cb) {
            std::move(cb).Run({web_contents_other_->GetWeakPtr()});
          });

  std::vector<QueryContextualizer::TabId> returned_tabs;
  delegate_->GetRelevantTabsForQuery(
      "test query", {},
      base::BindLambdaForTesting(
          [&](std::vector<QueryContextualizer::TabId> tab_ids) {
            returned_tabs = tab_ids;
          }));

  EXPECT_EQ(returned_tabs.size(), 1u);
  EXPECT_EQ(returned_tabs[0], mock_other_tab_.GetHandle().raw_value());
}

TEST_F(DesktopQueryContextualizerDelegateTest,
       GetRelevantTabs_PanelOpen_ActiveTabMissing_AppendsActiveTab) {
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_service_,
              GetRelevantTabsForConversationThread(_, _, _, _))
      .WillOnce(
          [this](const TabSelectionOptions& options,
                 const ConversationThread& conversation_thread,
                 const std::vector<GURL>& explicit_urls,
                 base::OnceCallback<void(
                     std::vector<base::WeakPtr<content::WebContents>>)> cb) {
            std::move(cb).Run({web_contents_other_->GetWeakPtr()});
          });

  std::vector<QueryContextualizer::TabId> returned_tabs;
  delegate_->GetRelevantTabsForQuery(
      "test query", {},
      base::BindLambdaForTesting(
          [&](std::vector<QueryContextualizer::TabId> tab_ids) {
            returned_tabs = tab_ids;
          }));

  EXPECT_EQ(returned_tabs.size(), 2u);
  EXPECT_EQ(returned_tabs[0], mock_other_tab_.GetHandle().raw_value());
  EXPECT_EQ(returned_tabs[1], mock_active_tab_.GetHandle().raw_value());
}

TEST_F(DesktopQueryContextualizerDelegateTest,
       GetRelevantTabs_PanelOpen_ActiveTabPresent_DoesNotDuplicateActiveTab) {
  ON_CALL(*mock_panel_host_, IsPanelOpenForContextualTask())
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_service_,
              GetRelevantTabsForConversationThread(_, _, _, _))
      .WillOnce(
          [this](const TabSelectionOptions& options,
                 const ConversationThread& conversation_thread,
                 const std::vector<GURL>& explicit_urls,
                 base::OnceCallback<void(
                     std::vector<base::WeakPtr<content::WebContents>>)> cb) {
            std::move(cb).Run({web_contents_active_->GetWeakPtr()});
          });

  std::vector<QueryContextualizer::TabId> returned_tabs;
  delegate_->GetRelevantTabsForQuery(
      "test query", {},
      base::BindLambdaForTesting(
          [&](std::vector<QueryContextualizer::TabId> tab_ids) {
            returned_tabs = tab_ids;
          }));

  EXPECT_EQ(returned_tabs.size(), 1u);
  EXPECT_EQ(returned_tabs[0], mock_active_tab_.GetHandle().raw_value());
}

}  // namespace
}  // namespace contextual_tasks
