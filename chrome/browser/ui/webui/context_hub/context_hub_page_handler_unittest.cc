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
#include "base/time/time.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
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
  MOCK_METHOD(void, SwitchToTab, (content::WebContents*, int64_t), (override));
};
#endif

class ContextHubPageHandlerTest : public testing::Test {
 public:
  ContextHubPageHandlerTest() {
    feature_list_.InitWithFeatures(
        {features::kContextHub, features::kMemoryBanks,
         browser::context_hub::mojom::kAutoTabGroups,
         browser::context_hub::mojom::kAutoTodos},
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
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockOptimizationGuideKeyedService>();
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

  MockOptimizationGuideKeyedService* GetMockOptimizationGuideService() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(&profile_));
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
  todo->set_importance_score(0.85f);

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      const std::optional<std::vector<context_hub::AutoTodoEntry>>&>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<context_hub::AutoTodoEntry>> result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).id, "Test Title");
  EXPECT_EQ(result->at(0).title, "Test Title");
  EXPECT_EQ(result->at(0).description, "Test Description");
  EXPECT_EQ(result->at(0).importance_score, 0.85f);
  EXPECT_EQ(result->at(0).status, context_hub::AutoTodoEntry::Status::kActive);
  ASSERT_TRUE(result->at(0).is_first_party());
  EXPECT_EQ(
      std::get<context_hub::FirstPartyData>(result->at(0).data).actionable_url,
      GURL("https://example.com/action"));
  EXPECT_TRUE(std::get<context_hub::FirstPartyData>(result->at(0).data)
                  .source_references.empty());
}

TEST_F(ContextHubPageHandlerTest, GenerateAutoTodos_WithSourceReferences) {
  personal_context::proto::AutoTodosResponse response;
  personal_context::proto::AutoTodoItem* todo = response.add_todos();
  todo->set_title("Test Title");
  todo->set_description("Test Description");
  todo->set_actionable_url("https://example.com/action2");

  personal_context::proto::SourceReference* ref_gmail1 =
      todo->add_source_references();
  ref_gmail1->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");

  personal_context::proto::SourceReference* ref_gmail2 =
      todo->add_source_references();
  ref_gmail2->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/456");

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      const std::optional<std::vector<context_hub::AutoTodoEntry>>&>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<context_hub::AutoTodoEntry>> result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).title, "Test Title");
  EXPECT_EQ(result->at(0).description, "Test Description");
  EXPECT_EQ(result->at(0).status, context_hub::AutoTodoEntry::Status::kActive);
  ASSERT_TRUE(result->at(0).is_first_party());
  const auto& first_party =
      std::get<context_hub::FirstPartyData>(result->at(0).data);
  EXPECT_EQ(first_party.actionable_url, GURL("https://example.com/action2"));
  ASSERT_EQ(first_party.source_references.size(), 2u);
  EXPECT_EQ(first_party.source_references[0],
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(first_party.source_references[1],
            GURL("https://mail.google.com/mail/u/0/#inbox/456"));
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
      const std::optional<std::vector<context_hub::AutoTodoEntry>>&>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<context_hub::AutoTodoEntry>> result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST(ContextHubMojomTraitsTest, StatusSerialization) {
  for (auto status : {context_hub::AutoTodoEntry::Status::kActive,
                      context_hub::AutoTodoEntry::Status::kCompleted,
                      context_hub::AutoTodoEntry::Status::kDismissed}) {
    context_hub::AutoTodoEntry::Status output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
                browser::context_hub::mojom::AutoTodoStatus>(status, output));
    EXPECT_EQ(output, status);
  }
}

TEST(ContextHubMojomTraitsTest, FirstPartyDataSerialization) {
  context_hub::FirstPartyData input;
  input.actionable_url = GURL("https://docs.google.com/doc/123");
  input.source_references.emplace_back(
      "https://mail.google.com/mail/u/0/#inbox/123");
  input.source_references.emplace_back(
      "https://mail.google.com/mail/u/0/#inbox/456");

  context_hub::FirstPartyData output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::FirstPartyData>(input, output));

  EXPECT_EQ(output.actionable_url, GURL("https://docs.google.com/doc/123"));
  ASSERT_EQ(output.source_references.size(), 2u);
  EXPECT_EQ(output.source_references[0],
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(output.source_references[1],
            GURL("https://mail.google.com/mail/u/0/#inbox/456"));
}

TEST(ContextHubMojomTraitsTest, ThirdPartyDataSerialization) {
  context_hub::ThirdPartyData input;
  input.tab_id = 98765;
  input.last_active_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000);

  context_hub::ThirdPartyData output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::ThirdPartyData>(input, output));

  EXPECT_EQ(output.tab_id, 98765);
  EXPECT_EQ(output.last_active_timestamp,
            base::Time::FromMillisecondsSinceUnixEpoch(1700000000000));
}

TEST(ContextHubMojomTraitsTest, AutoTodoDataSerialization_FirstParty) {
  context_hub::FirstPartyData first_party;
  first_party.actionable_url = GURL("https://docs.google.com/doc/123");
  first_party.source_references.emplace_back(
      "https://mail.google.com/mail/u/0/#inbox/123");
  std::variant<context_hub::FirstPartyData, context_hub::ThirdPartyData> input =
      std::move(first_party);

  std::variant<context_hub::FirstPartyData, context_hub::ThirdPartyData> output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::AutoTodoData>(input, output));

  ASSERT_TRUE(std::holds_alternative<context_hub::FirstPartyData>(output));
  const auto& out_first_party = std::get<context_hub::FirstPartyData>(output);
  EXPECT_EQ(out_first_party.actionable_url,
            GURL("https://docs.google.com/doc/123"));
  ASSERT_EQ(out_first_party.source_references.size(), 1u);
  EXPECT_EQ(out_first_party.source_references[0],
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));
}

TEST(ContextHubMojomTraitsTest, AutoTodoDataSerialization_ThirdParty) {
  context_hub::ThirdPartyData third_party;
  third_party.tab_id = 54321;
  third_party.last_active_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000);
  std::variant<context_hub::FirstPartyData, context_hub::ThirdPartyData> input =
      std::move(third_party);

  std::variant<context_hub::FirstPartyData, context_hub::ThirdPartyData> output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::AutoTodoData>(input, output));

  ASSERT_TRUE(std::holds_alternative<context_hub::ThirdPartyData>(output));
  const auto& out_third_party = std::get<context_hub::ThirdPartyData>(output);
  EXPECT_EQ(out_third_party.tab_id, 54321);
  EXPECT_EQ(out_third_party.last_active_timestamp,
            base::Time::FromMillisecondsSinceUnixEpoch(1700000000000));
}

TEST(ContextHubMojomTraitsTest, AutoTodoItemSerialization_FirstPartyData) {
  context_hub::AutoTodoEntry input;
  input.id = "todo_1";
  input.title = "Review Document";
  input.description = "Review the quarterly report";
  input.importance_score = 0.95f;
  input.status = context_hub::AutoTodoEntry::Status::kActive;

  context_hub::FirstPartyData first_party;
  first_party.actionable_url = GURL("https://docs.google.com/doc/123");
  first_party.source_references.emplace_back(
      "https://mail.google.com/mail/u/0/#inbox/123");
  input.data = std::move(first_party);

  context_hub::AutoTodoEntry output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::AutoTodoItem>(input, output));

  EXPECT_EQ(output.id, input.id);
  EXPECT_EQ(output.title, input.title);
  EXPECT_EQ(output.description, input.description);
  EXPECT_EQ(output.status, context_hub::AutoTodoEntry::Status::kActive);
  EXPECT_FLOAT_EQ(output.importance_score, 0.95f);
  ASSERT_TRUE(output.is_first_party());
  EXPECT_EQ(std::get<context_hub::FirstPartyData>(output.data).actionable_url,
            GURL("https://docs.google.com/doc/123"));
  ASSERT_EQ(std::get<context_hub::FirstPartyData>(output.data)
                .source_references.size(),
            1u);
  EXPECT_EQ(
      std::get<context_hub::FirstPartyData>(output.data).source_references[0],
      GURL("https://mail.google.com/mail/u/0/#inbox/123"));
}

TEST(ContextHubMojomTraitsTest, AutoTodoItemSerialization_ThirdPartyData) {
  context_hub::AutoTodoEntry input;
  input.id = "todo_2";
  input.title = "Tab Todo";
  input.description = "Resume working on tab";
  input.importance_score = 0.75f;
  input.status = context_hub::AutoTodoEntry::Status::kCompleted;

  context_hub::ThirdPartyData third_party;
  third_party.tab_id = 12345;
  third_party.last_active_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000);
  input.data = std::move(third_party);

  context_hub::AutoTodoEntry output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::AutoTodoItem>(input, output));

  EXPECT_EQ(output.id, input.id);
  EXPECT_EQ(output.title, input.title);
  EXPECT_EQ(output.description, input.description);
  EXPECT_EQ(output.status, context_hub::AutoTodoEntry::Status::kCompleted);
  EXPECT_FLOAT_EQ(output.importance_score, 0.75f);
  ASSERT_TRUE(output.is_third_party());
  EXPECT_EQ(output.tab_id(), 12345);
  EXPECT_EQ(
      std::get<context_hub::ThirdPartyData>(output.data).last_active_timestamp,
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000));
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
  service->SaveTab(GURL("https://example.com/tab"), "Tab Title", "Page text",
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
                   "Page text 1", save_tab_future1.GetCallback());
  ASSERT_TRUE(save_tab_future1.Wait());

  base::test::TestFuture<void> save_tab_future2;
  service->SaveTab(GURL("https://example.com/tab2"), "Tab Title 2",
                   "Page text 2", save_tab_future2.GetCallback());
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
TEST_F(ContextHubPageHandlerTest, SwitchToTab) {
  EXPECT_CALL(*mock_tab_provider_, SwitchToTab(_, 42)).Times(1);

  handler_->SwitchToTab(42);
}

TEST_F(ContextHubPageHandlerTest, GetTabs_NoTabs) {
  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(std::vector<content::WebContents*>{}));

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabInfoPtr>>
      future;
  handler_->GetTabs(future.GetCallback());

  std::vector<browser::context_hub::mojom::TabInfoPtr> tabs = future.Take();
  EXPECT_TRUE(tabs.empty());
}

TEST_F(ContextHubPageHandlerTest, GetTabs_WithTabs) {
  std::vector<std::unique_ptr<content::WebContents>> test_tabs;
  std::vector<content::WebContents*> raw_test_tabs;
  for (int i = 0; i < 3; ++i) {
    auto tab =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    sessions::SessionTabHelper::CreateForWebContents(
        tab.get(), sessions::SessionTabHelper::DelegateLookup());
    raw_test_tabs.push_back(tab.get());
    test_tabs.push_back(std::move(tab));
  }

  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(raw_test_tabs));

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabInfoPtr>>
      future;
  handler_->GetTabs(future.GetCallback());

  std::vector<browser::context_hub::mojom::TabInfoPtr> tabs = future.Take();
  EXPECT_EQ(tabs.size(), 3u);
}

TEST_F(ContextHubPageHandlerTest, RetrieveAndGroupTabs_NoTabs) {
  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(std::vector<content::WebContents*>{}));

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabGroupPtr>,
                         std::vector<browser::context_hub::mojom::TabInfoPtr>,
                         browser::context_hub::mojom::ChatMessagePtr>
      future;
  handler_->RetrieveAndGroupTabs(
      "",
      future.GetCallback<std::vector<browser::context_hub::mojom::TabGroupPtr>,
                         std::vector<browser::context_hub::mojom::TabInfoPtr>,
                         browser::context_hub::mojom::ChatMessagePtr>());

  auto [groups, ungrouped_tabs, llm_response] = future.Take();
  EXPECT_TRUE(groups.empty());
  EXPECT_TRUE(ungrouped_tabs.empty());
  EXPECT_FALSE(llm_response);
}

TEST_F(ContextHubPageHandlerTest, RetrieveAndGroupTabs_WithTabs) {
  std::vector<std::unique_ptr<content::WebContents>> test_tabs;
  std::vector<content::WebContents*> raw_test_tabs;
  std::vector<int64_t> tab_ids;
  for (int i = 0; i < 5; ++i) {
    auto tab =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    sessions::SessionTabHelper::CreateForWebContents(
        tab.get(), sessions::SessionTabHelper::DelegateLookup());
    tab_ids.push_back(sessions::SessionTabHelper::IdForTab(tab.get()).id());
    raw_test_tabs.push_back(tab.get());
    test_tabs.push_back(std::move(tab));
  }

  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(raw_test_tabs));

  EXPECT_CALL(
      *GetMockOptimizationGuideService(),
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([tab_ids](
                    optimization_guide::ModelBasedCapabilityKey feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const optimization_guide::ModelExecutionOptions& options,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultCallback
                            callback) {
        const auto& request =
            static_cast<const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
        EXPECT_EQ(request.user_command(), "test command");

        optimization_guide::proto::ContextHubResponse response;
        optimization_guide::proto::GroupResponse* group_response =
            response.mutable_group_response();
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Group 1");
        group1->add_tab_ids(tab_ids[0]);
        group1->add_tab_ids(tab_ids[1]);

        optimization_guide::proto::TabGroupMinimal* group2 =
            group_response->add_minimal_tab_groups();
        group2->set_label("Group 2");
        group2->add_tab_ids(tab_ids[2]);
        group2->add_tab_ids(tab_ids[3]);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabGroupPtr>,
                         std::vector<browser::context_hub::mojom::TabInfoPtr>,
                         browser::context_hub::mojom::ChatMessagePtr>
      future;
  handler_->RetrieveAndGroupTabs(
      "test command",
      future.GetCallback<std::vector<browser::context_hub::mojom::TabGroupPtr>,
                         std::vector<browser::context_hub::mojom::TabInfoPtr>,
                         browser::context_hub::mojom::ChatMessagePtr>());

  auto [groups, ungrouped_tabs, llm_response] = future.Take();
  EXPECT_EQ(groups.size(), 2u);
  size_t total_tabs = ungrouped_tabs.size();
  for (const auto& group : groups) {
    total_tabs += group->tabs.size();
    EXPECT_GE(group->tabs.size(), 2u);
  }
  EXPECT_EQ(total_tabs, 5u);
  EXPECT_FALSE(llm_response);
}

TEST_F(ContextHubPageHandlerTest, GetExistingTabGroupsAndChats_WithGroups) {
  std::vector<std::unique_ptr<content::WebContents>> test_tabs;
  std::vector<content::WebContents*> raw_test_tabs;
  std::vector<int32_t> tab_ids;
  for (int i = 0; i < 3; ++i) {
    auto tab =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    sessions::SessionTabHelper::CreateForWebContents(
        tab.get(), sessions::SessionTabHelper::DelegateLookup());
    tab_ids.push_back(sessions::SessionTabHelper::IdForTab(tab.get()).id());
    raw_test_tabs.push_back(tab.get());
    test_tabs.push_back(std::move(tab));
  }

  // 1. Group tabs so that service stores tab groups.
  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(raw_test_tabs));

  EXPECT_CALL(
      *GetMockOptimizationGuideService(),
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([tab_ids](
                    optimization_guide::ModelBasedCapabilityKey feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const optimization_guide::ModelExecutionOptions& options,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultCallback
                            callback) {
        optimization_guide::proto::ContextHubResponse response;
        optimization_guide::proto::GroupResponse* group_response =
            response.mutable_group_response();
        optimization_guide::proto::TabGroupMinimal* group1 =
            group_response->add_minimal_tab_groups();
        group1->set_label("Existing Group");
        group1->add_tab_ids(tab_ids[0]);
        group1->add_tab_ids(tab_ids[1]);

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabGroupPtr>,
                         std::vector<browser::context_hub::mojom::TabInfoPtr>,
                         browser::context_hub::mojom::ChatMessagePtr>
      group_future;
  handler_->RetrieveAndGroupTabs(
      "",
      group_future
          .GetCallback<std::vector<browser::context_hub::mojom::TabGroupPtr>,
                       std::vector<browser::context_hub::mojom::TabInfoPtr>,
                       browser::context_hub::mojom::ChatMessagePtr>());
  EXPECT_TRUE(group_future.Wait());

  // 2. Add chat history turn.
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);
  service->AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "Hello");

  // 3. Call GetExistingTabGroupsAndChats and verify output.
  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(raw_test_tabs));

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::TabGroupPtr>,
      std::vector<browser::context_hub::mojom::TabInfoPtr>,
      std::vector<browser::context_hub::mojom::ChatMessagePtr>>
      existing_future;
  handler_->GetExistingTabGroupsAndChats(
      existing_future.GetCallback<
          std::vector<browser::context_hub::mojom::TabGroupPtr>,
          std::vector<browser::context_hub::mojom::TabInfoPtr>,
          std::vector<browser::context_hub::mojom::ChatMessagePtr>>());

  auto [groups, ungrouped_tabs, chat_history] = existing_future.Take();
  ASSERT_EQ(groups.size(), 1u);
  EXPECT_EQ(groups[0]->label, "Existing Group");
  EXPECT_EQ(groups[0]->tabs.size(), 2u);

  ASSERT_EQ(ungrouped_tabs.size(), 1u);
  EXPECT_EQ(ungrouped_tabs[0]->id, tab_ids[2]);

  ASSERT_EQ(chat_history.size(), 1u);
  EXPECT_EQ(chat_history[0]->content, "Hello");
  EXPECT_EQ(chat_history[0]->role,
            browser::context_hub::mojom::ChatRole::kUser);
}

TEST_F(ContextHubPageHandlerTest, GetExistingTabGroupsAndChats_NoGroups) {
  EXPECT_CALL(*mock_tab_provider_, GetTabs(_))
      .WillOnce(testing::Return(std::vector<content::WebContents*>{}));

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::TabGroupPtr>,
      std::vector<browser::context_hub::mojom::TabInfoPtr>,
      std::vector<browser::context_hub::mojom::ChatMessagePtr>>
      future;
  handler_->GetExistingTabGroupsAndChats(
      future.GetCallback<
          std::vector<browser::context_hub::mojom::TabGroupPtr>,
          std::vector<browser::context_hub::mojom::TabInfoPtr>,
          std::vector<browser::context_hub::mojom::ChatMessagePtr>>());

  auto [groups, ungrouped_tabs, chat_history] = future.Take();
  EXPECT_TRUE(groups.empty());
  EXPECT_TRUE(ungrouped_tabs.empty());
  EXPECT_TRUE(chat_history.empty());
}
#endif

TEST_F(ContextHubPageHandlerTest, ClearTabGroupChatHistory) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  service->AddTabGroupChatHistoryTurn(
      optimization_guide::proto::ChatHistoryTurn::ROLE_USER, "Message");
  EXPECT_EQ(service->GetTabGroupChatHistory().size(), 1u);

  base::test::TestFuture<void> future;
  handler_->ClearTabGroupChatHistory(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_TRUE(service->GetTabGroupChatHistory().empty());
}

TEST_F(ContextHubPageHandlerTest, ClearTabGroups) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> future;
  handler_->ClearTabGroups(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  base::test::TestFuture<std::vector<TabGroupEntry>> stored_groups_future;
  service->GetTabGroups(stored_groups_future.GetCallback());
  EXPECT_TRUE(stored_groups_future.Get().empty());
}

TEST_F(ContextHubPageHandlerTest, AskGeminiWithContext_Success) {
  EXPECT_CALL(
      *GetMockOptimizationGuideService(),
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions& options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        const auto& request =
            static_cast<const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
        EXPECT_EQ(request.request_type(),
                  optimization_guide::proto::
                      CONTEXT_HUB_REQUEST_TYPE_MEMORY_BANK_CHAT);
        EXPECT_EQ(request.user_command(), "Summarize memories");

        optimization_guide::proto::ContextHubResponse response;
        response.mutable_memory_bank_chat_response()->set_text_response(
            "Gemini response for prompt.");

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<browser::context_hub::mojom::ChatMessagePtr> future;
  handler_->AskGeminiWithContext("Summarize memories", {1, 2},
                                 future.GetCallback());

  browser::context_hub::mojom::ChatMessagePtr response = future.Take();
  ASSERT_TRUE(response);
  EXPECT_EQ(response->role, browser::context_hub::mojom::ChatRole::kAssistant);
  EXPECT_EQ(response->content, "Gemini response for prompt.");
}

TEST_F(ContextHubPageHandlerTest, AskGeminiWithContext_WithSelectedEntries) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> save_future;
  service->SaveTab(GURL("https://example.com/test"), "Test Page",
                   "Page content", save_future.GetCallback());
  ASSERT_TRUE(save_future.Wait());

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      all_entries_future;
  handler_->GetAllMemoryBankEntries(all_entries_future.GetCallback());
  auto entries = all_entries_future.Take();
  ASSERT_EQ(entries.size(), 1u);
  int64_t entry_id = entries[0]->id;

  EXPECT_CALL(
      *GetMockOptimizationGuideService(),
      ExecuteModel(optimization_guide::ModelBasedCapabilityKey::kContextHub, _,
                   _, _))
      .WillOnce([entry_id](
                    optimization_guide::ModelBasedCapabilityKey feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const optimization_guide::ModelExecutionOptions& options,
                    optimization_guide::
                        OptimizationGuideModelExecutionResultCallback
                            callback) {
        const auto& request =
            static_cast<const optimization_guide::proto::ContextHubRequest&>(
                request_metadata);
        EXPECT_EQ(request.request_type(),
                  optimization_guide::proto::
                      CONTEXT_HUB_REQUEST_TYPE_MEMORY_BANK_CHAT);
        EXPECT_EQ(request.user_command(), "Summarize");
        ASSERT_EQ(request.entry_items_size(), 1);
        EXPECT_TRUE(request.entry_items(0).has_memory_bank_entry());
        EXPECT_EQ(request.entry_items(0).memory_bank_entry().id(), entry_id);
        EXPECT_EQ(request.entry_items(0).memory_bank_entry().tab_title(),
                  "Test Page");
        EXPECT_EQ(request.entry_items(0).memory_bank_entry().url(),
                  "https://example.com/test");

        optimization_guide::proto::ContextHubResponse response;
        response.mutable_memory_bank_chat_response()->set_text_response(
            "Summary of Test Page.");

        optimization_guide::proto::Any any_response;
        any_response.set_type_url(
            "type.googleapis.com/optimization_guide.proto.ContextHubResponse");
        response.SerializeToString(any_response.mutable_value());

        std::move(callback).Run(
            optimization_guide::OptimizationGuideModelExecutionResult(
                base::ok(std::move(any_response)), nullptr),
            nullptr);
      });

  base::test::TestFuture<browser::context_hub::mojom::ChatMessagePtr>
      ask_future;
  handler_->AskGeminiWithContext("Summarize", {entry_id},
                                 ask_future.GetCallback());

  browser::context_hub::mojom::ChatMessagePtr response = ask_future.Take();
  ASSERT_TRUE(response);
  EXPECT_EQ(response->role, browser::context_hub::mojom::ChatRole::kAssistant);
  EXPECT_EQ(response->content, "Summary of Test Page.");
}

TEST_F(ContextHubPageHandlerTest, ClearTodoFeedbacks) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  // Add a feedback item.
  auto feedback = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback->todo_id = "todo_1";
  feedback->liked = true;

  base::test::TestFuture<void> set_future;
  handler_->SetTodoFeedback(std::move(feedback), set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  // Verify that the feedback item was added.
  EXPECT_EQ(1u, service->GetTodoFeedbacks().size());

  base::test::TestFuture<void> clear_future;
  handler_->ClearTodoFeedbacks(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Wait());
  // Verify that the feedback item was cleared.
  EXPECT_TRUE(service->GetTodoFeedbacks().empty());
}

TEST_F(ContextHubPageHandlerTest, DeleteTodoFeedback) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  auto feedback1 = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback1->todo_id = "todo_1";
  feedback1->liked = true;

  auto feedback2 = browser::context_hub::mojom::AutoTodoItemFeedback::New();
  feedback2->todo_id = "todo_2";
  feedback2->liked = false;

  // Add two feedback items.
  base::test::TestFuture<void> set_future1;
  handler_->SetTodoFeedback(std::move(feedback1), set_future1.GetCallback());
  EXPECT_TRUE(set_future1.Wait());

  base::test::TestFuture<void> set_future2;
  handler_->SetTodoFeedback(std::move(feedback2), set_future2.GetCallback());
  EXPECT_TRUE(set_future2.Wait());

  // Verify that the feedback items were added.
  EXPECT_EQ(2u, service->GetTodoFeedbacks().size());

  base::test::TestFuture<void> clear_future;
  handler_->DeleteTodoFeedback("todo_1", clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Wait());

  // Verify that the feedback item for todo_1 was cleared.
  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::AutoTodoItemFeedbackPtr>>
      get_future;
  handler_->GetTodoFeedbacks(get_future.GetCallback());
  std::vector<browser::context_hub::mojom::AutoTodoItemFeedbackPtr> feedbacks =
      get_future.Take();
  ASSERT_EQ(1u, feedbacks.size());
  EXPECT_EQ("todo_2", feedbacks[0]->todo_id);
  EXPECT_FALSE(feedbacks[0]->liked);
}

}  // namespace
}  // namespace context_hub
