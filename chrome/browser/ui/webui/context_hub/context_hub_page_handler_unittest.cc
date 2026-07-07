// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "components/sessions/content/session_tab_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

#if !BUILDFLAG(IS_ANDROID)
class MockTabProvider : public ContextHubPageHandler::TabProvider {
 public:
  MOCK_METHOD(std::vector<content::WebContents*>,
              GetTabs,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void,
              SwitchToTab,
              (content::WebContents*, int32_t),
              (override));
};
#endif

class ContextHubPageHandlerTest : public testing::Test {
 public:
  ContextHubPageHandlerTest() {
    feature_list_.InitWithFeatures(
        {features::kContextHub, features::kAutoTodos, features::kMemoryBanks},
        {});
  }

  void SetUp() override {
    testing::Test::SetUp();

    PersonalContextServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              personal_context::MockPersonalContextService>();
        }));

#if !BUILDFLAG(IS_ANDROID)
    auto mock_tab_provider = std::make_unique<MockTabProvider>();
    mock_tab_provider_ = mock_tab_provider.get();
    handler_ = std::make_unique<ContextHubPageHandler>(
        mojo::PendingReceiver<browser::context_hub::mojom::PageHandler>(),
        &profile_, nullptr, std::move(mock_tab_provider));
#else
    handler_ = std::make_unique<ContextHubPageHandler>(
        mojo::PendingReceiver<browser::context_hub::mojom::PageHandler>(),
        &profile_, nullptr, nullptr);
#endif
  }

  void TearDown() override {
#if !BUILDFLAG(IS_ANDROID)
    mock_tab_provider_ = nullptr;
#endif
    handler_.reset();
    testing::Test::TearDown();
  }

 protected:
  personal_context::MockPersonalContextService* GetMockService() {
    return static_cast<personal_context::MockPersonalContextService*>(
        PersonalContextServiceFactory::GetForProfile(&profile_));
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockTabProvider> mock_tab_provider_ = nullptr;
#endif
  std::unique_ptr<ContextHubPageHandler> handler_;
};

TEST_F(ContextHubPageHandlerTest, GenerateAutoTodos_Success) {
  personal_context::proto::AutoTodosResponse response;
  personal_context::proto::AutoTodoItem* todo = response.add_todos();
  todo->set_title("Test Title");
  todo->set_description("Test Description");
  todo->set_actionable_url("https://example.com/action");

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0)->title, "Test Title");
  EXPECT_EQ(result->at(0)->description, "Test Description");
  EXPECT_EQ(result->at(0)->actionable_url, GURL("https://example.com/action"));
  EXPECT_TRUE(result->at(0)->source_references.empty());
}

TEST_F(ContextHubPageHandlerTest, GenerateAutoTodos_WithSourceReferences) {
  personal_context::proto::AutoTodosResponse response;
  personal_context::proto::AutoTodoItem* todo = response.add_todos();
  todo->set_title("Test Title");
  todo->set_description("Test Description");
  todo->set_actionable_url("https://example.com/action2");

  personal_context::proto::SourceReference* ref_gmail =
      todo->add_source_references();
  ref_gmail->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");

  personal_context::proto::SourceReference* ref_photos =
      todo->add_source_references();
  ref_photos->mutable_photos()->set_photos_url(
      "https://photos.google.com/photo/456");

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0)->title, "Test Title");
  EXPECT_EQ(result->at(0)->description, "Test Description");
  EXPECT_EQ(result->at(0)->actionable_url, GURL("https://example.com/action2"));

  ASSERT_EQ(result->at(0)->source_references.size(), 2u);

  const browser::context_hub::mojom::SourceReferencePtr& mojo_ref1 =
      result->at(0)->source_references[0];
  ASSERT_TRUE(mojo_ref1->is_gmail());
  EXPECT_EQ(mojo_ref1->get_gmail()->message_url,
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));

  const browser::context_hub::mojom::SourceReferencePtr& mojo_ref2 =
      result->at(0)->source_references[1];
  ASSERT_TRUE(mojo_ref2->is_photos());
  EXPECT_EQ(mojo_ref2->get_photos()->photos_url,
            GURL("https://photos.google.com/photo/456"));
}

TEST_F(ContextHubPageHandlerTest, GenerateAutoTodos_Failure) {
  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::unexpected(
              personal_context::ContextMemoryError::FromExecutionError(
                  personal_context::ContextMemoryError::ExecutionError::
                      kUnknown)))));

  base::test::TestFuture<
      std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankEntries_Empty) {
  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      future;
  handler_->GetAllMemoryBankEntries(future.GetCallback());

  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> result =
      future.Take();
  EXPECT_TRUE(result.empty());
}

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankEntries_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> save_tab_future;
  service->SaveTab(GURL("https://example.com/tab"), "Tab Title",
                   save_tab_future.GetCallback());
  ASSERT_TRUE(save_tab_future.Wait());

  base::test::TestFuture<void> save_selection_future;
  service->SaveTextSelection(GURL("https://example.com/select"),
                             "Selection Title", "Selected Text Detail",
                             save_selection_future.GetCallback());
  ASSERT_TRUE(save_selection_future.Wait());

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      future;
  handler_->GetAllMemoryBankEntries(future.GetCallback());

  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> result =
      future.Take();

  ASSERT_EQ(result.size(), 2u);

  const browser::context_hub::mojom::MemoryBankEntryPtr* tab_entry = nullptr;
  const browser::context_hub::mojom::MemoryBankEntryPtr* text_entry = nullptr;
  for (const auto& entry : result) {
    if (entry->type == browser::context_hub::mojom::EntryType::kTab) {
      tab_entry = &entry;
    } else if (entry->type ==
               browser::context_hub::mojom::EntryType::kTextSelection) {
      text_entry = &entry;
    }
  }

  ASSERT_TRUE(tab_entry);
  EXPECT_EQ((*tab_entry)->url, GURL("https://example.com/tab"));
  EXPECT_EQ((*tab_entry)->tab_title, "Tab Title");
  EXPECT_FALSE((*tab_entry)->timestamp.is_null());

  ASSERT_TRUE(text_entry);
  EXPECT_EQ((*text_entry)->url, GURL("https://example.com/select"));
  EXPECT_EQ((*text_entry)->tab_title, "Selection Title");
  EXPECT_EQ((*text_entry)->selected_text, "Selected Text Detail");
  EXPECT_FALSE((*text_entry)->timestamp.is_null());
}

TEST_F(ContextHubPageHandlerTest, DeleteMemoryBankEntries_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> save_tab_future1;
  service->SaveTab(GURL("https://example.com/tab1"), "Tab Title 1",
                   save_tab_future1.GetCallback());
  ASSERT_TRUE(save_tab_future1.Wait());

  base::test::TestFuture<void> save_tab_future2;
  service->SaveTab(GURL("https://example.com/tab2"), "Tab Title 2",
                   save_tab_future2.GetCallback());
  ASSERT_TRUE(save_tab_future2.Wait());

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      get_all_future1;
  handler_->GetAllMemoryBankEntries(get_all_future1.GetCallback());
  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> entries1 =
      get_all_future1.Take();
  ASSERT_EQ(entries1.size(), 2u);

  std::vector<int64_t> entry_ids = {entries1[0]->id, entries1[1]->id};
  base::test::TestFuture<void> delete_future;
  handler_->DeleteMemoryBankEntries(entry_ids, delete_future.GetCallback());
  ASSERT_TRUE(delete_future.Wait());

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      get_all_future2;
  handler_->GetAllMemoryBankEntries(get_all_future2.GetCallback());
  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> entries2 =
      get_all_future2.Take();
  EXPECT_TRUE(entries2.empty());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextHubPageHandlerTest, GetTabs) {
  std::unique_ptr<content::WebContents> tab1 =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  content::WebContentsTester::For(tab1.get())->SetTitle(u"Tab One");
  sessions::SessionTabHelper::CreateForWebContents(
      tab1.get(), sessions::SessionTabHelper::DelegateLookup());

  std::unique_ptr<content::WebContents> tab2 =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  content::WebContentsTester::For(tab2.get())->SetTitle(u"Tab Two");
  sessions::SessionTabHelper::CreateForWebContents(
      tab2.get(), sessions::SessionTabHelper::DelegateLookup());

  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(std::vector<content::WebContents*>{tab1.get(), tab2.get()}));

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabInfoPtr>>
      future;
  handler_->GetTabs(future.GetCallback());

  std::vector<browser::context_hub::mojom::TabInfoPtr> tabs = future.Take();
  ASSERT_EQ(tabs.size(), 2u);

  EXPECT_EQ(tabs[0]->title, "Tab One");
  EXPECT_EQ(tabs[0]->url, tab1->GetLastCommittedURL());
  EXPECT_NE(tabs[0]->id, 0);

  EXPECT_EQ(tabs[1]->title, "Tab Two");
  EXPECT_EQ(tabs[1]->url, tab2->GetLastCommittedURL());
  EXPECT_NE(tabs[1]->id, 0);
}

TEST_F(ContextHubPageHandlerTest, SwitchToTab) {
  EXPECT_CALL(*mock_tab_provider_, SwitchToTab(_, 42))
      .Times(1);

  handler_->SwitchToTab(42);
}

TEST_F(ContextHubPageHandlerTest, ClusterTabs_NoTabs) {
  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(std::vector<content::WebContents*>{}));

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::TabClusterPtr>,
      std::vector<int32_t>>
      future;
  handler_->ClusterTabs(
      future
          .GetCallback<std::vector<browser::context_hub::mojom::TabClusterPtr>,
                       const std::vector<int32_t>&>());

  auto [clusters, ungrouped_tab_ids] = future.Take();
  EXPECT_TRUE(clusters.empty());
  EXPECT_TRUE(ungrouped_tab_ids.empty());
}

TEST_F(ContextHubPageHandlerTest, ClusterTabs_WithTabs) {
  std::vector<std::unique_ptr<content::WebContents>> test_tabs;
  std::vector<content::WebContents*> raw_test_tabs;
  for (int i = 0; i < 5; ++i) {
    auto tab = content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    sessions::SessionTabHelper::CreateForWebContents(
        tab.get(), sessions::SessionTabHelper::DelegateLookup());
    raw_test_tabs.push_back(tab.get());
    test_tabs.push_back(std::move(tab));
  }

  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(raw_test_tabs));

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::TabClusterPtr>,
      std::vector<int32_t>>
      future;
  handler_->ClusterTabs(
      future
          .GetCallback<std::vector<browser::context_hub::mojom::TabClusterPtr>,
                       const std::vector<int32_t>&>());

  auto [clusters, ungrouped_tab_ids] = future.Take();

  size_t clustered_tabs_count = 0;
  for (const auto& cluster : clusters) {
    clustered_tabs_count += cluster->tab_ids.size();
    EXPECT_GE(cluster->tab_ids.size(), 2u);
  }
  EXPECT_EQ(clustered_tabs_count + ungrouped_tab_ids.size(), 5u);
}
#endif

}  // namespace
}  // namespace context_hub
