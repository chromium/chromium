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
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/prefs.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
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
using ::testing::IsEmpty;
using ::testing::Not;

#if !BUILDFLAG(IS_ANDROID)
class MockTabProvider : public ContextHubPageHandler::TabProvider {
 public:
  MOCK_METHOD(std::vector<content::WebContents*>, GetTabs, (), (override));
  MOCK_METHOD(std::vector<content::WebContents*>,
              GetUngroupedTabs,
              (),
              (override));
  MOCK_METHOD(void, SwitchToTab, (int64_t), (override));
  MOCK_METHOD(void, CloseTab, (int64_t), (override));
  MOCK_METHOD(bool,
              ConfirmTabGroups,
              (base::span<const context_hub::TabGroupEntry>),
              (override));
  MOCK_METHOD(void,
              RemoveGroupFromTabstripIfOpen,
              (const base::Uuid&),
              (override));
  MOCK_METHOD(void,
              UngroupGroupFromTabstripIfOpen,
              (const base::Uuid&),
              (override));
};
#endif

class MockPage : public browser::context_hub::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<browser::context_hub::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Flush() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              OnAutoTodosChanged,
              (const std::vector<context_hub::AutoTodoEntry>&),
              (override));
  MOCK_METHOD(void,
              OnFirstPartyAutoTodosGenerationStateChanged,
              (bool),
              (override));
  MOCK_METHOD(void,
              OnThirdPartyAutoTodosGenerationStateChanged,
              (bool),
              (override));

 private:
  mojo::Receiver<browser::context_hub::mojom::Page> receiver_{this};
};

class ContextHubPageHandlerTest : public testing::Test {
 public:
  ContextHubPageHandlerTest()
      : feature_list_(CreateScopedFeatureList()),
        create_services_subscription_(
            BrowserContextDependencyManager::GetInstance()
                ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                    &ContextHubPageHandlerTest::
                        OnWillCreateBrowserContextKeyedServices,
                    base::Unretained(this)))) {}

  void OnWillCreateBrowserContextKeyedServices(
      content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    profile->GetPrefs()->SetTime(prefs::kContextHubLastAutoTodosGenerationTime,
                                 base::Time::Now());

    tab_groups::TabGroupSyncServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<tab_groups::FakeTabGroupSyncService>();
        }));
    PersonalContextServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        browser_context,
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              personal_context::MockPersonalContextService>();
        }));
    OptimizationGuideKeyedServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_context,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockOptimizationGuideKeyedService>();
            }));
  }

  void SetUp() override {
    testing::Test::SetUp();

    profile_.GetPrefs()->SetTime(prefs::kContextHubLastAutoTodosGenerationTime,
                                 base::Time::Now());

#if !BUILDFLAG(IS_ANDROID)
    auto mock_tab_provider = std::make_unique<MockTabProvider>();
    mock_tab_provider_ = mock_tab_provider.get();
    handler_ = std::make_unique<ContextHubPageHandler>(
        mock_page_.BindAndGetRemote(),
        mojo::PendingReceiver<browser::context_hub::mojom::PageHandler>(),
        &profile_, nullptr, std::move(mock_tab_provider));
#else
    handler_ = std::make_unique<ContextHubPageHandler>(
        mock_page_.BindAndGetRemote(),
        mojo::PendingReceiver<browser::context_hub::mojom::PageHandler>(),
        &profile_, nullptr, nullptr);
#endif
    mock_page_.Flush();
  }

  void TearDown() override {
#if !BUILDFLAG(IS_ANDROID)
    mock_tab_provider_ = nullptr;
#endif
    handler_.reset();
    testing::Test::TearDown();
  }

 protected:
  static base::test::ScopedFeatureList CreateScopedFeatureList() {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kContextHub, features::kMemoryBanks,
         browser::context_hub::mojom::kAutoTabGroups,
         browser::context_hub::mojom::kAutoTodos,
         optimization_guide::features::kOptimizationHints},
        {});
    return feature_list;
  }

  personal_context::MockPersonalContextService* GetMockService() {
    return static_cast<personal_context::MockPersonalContextService*>(
        PersonalContextServiceFactory::GetForProfile(&profile_));
  }

  MockOptimizationGuideKeyedService* GetMockOptimizationGuideService() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(&profile_));
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::CallbackListSubscription create_services_subscription_;

  struct EnsureFactories {
    EnsureFactories() { ContextHubServiceFactory::GetInstance(); }
  } ensure_factories_;
  TestingProfile profile_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockTabProvider> mock_tab_provider_ = nullptr;
#endif
  MockPage mock_page_;
  std::unique_ptr<ContextHubPageHandler> handler_;
};

TEST_F(ContextHubPageHandlerTest, GenerateFirstPartyAutoTodos_Success) {
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

  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(true));
  // Notification after adding the todos.
  EXPECT_CALL(mock_page_, OnAutoTodosChanged(Not(IsEmpty())))
      .WillOnce([](const std::vector<context_hub::AutoTodoEntry>& todos) {
        ASSERT_EQ(todos.size(), 1u);
        EXPECT_EQ(todos[0].title, "Test Title");
        EXPECT_EQ(todos[0].description, "Test Description");
        EXPECT_FLOAT_EQ(todos[0].importance_score, 0.85f);
        ASSERT_TRUE(todos[0].is_first_party());
        const auto& first_party =
            std::get<context_hub::FirstPartyData>(todos[0].data);
        EXPECT_EQ(first_party.actionable_url,
                  GURL("https://example.com/action"));
        EXPECT_TRUE(first_party.source_references.empty());
      });
  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  handler_->GenerateFirstPartyAutoTodos(future.GetCallback());
  EXPECT_TRUE(future.Get());
  mock_page_.Flush();
}

TEST_F(ContextHubPageHandlerTest,
       GenerateFirstPartyAutoTodos_WithSourceReferences) {
  personal_context::proto::AutoTodosResponse response;
  personal_context::proto::AutoTodoItem* todo = response.add_todos();
  todo->set_title("Test Title");
  todo->set_description("Test Description");
  todo->set_actionable_url("https://example.com/action2");

  personal_context::proto::SourceReference* ref_gmail1 =
      todo->add_source_references();
  ref_gmail1->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");
  ref_gmail1->mutable_gmail()->set_subject("Subject 1");

  personal_context::proto::SourceReference* ref_gmail2 =
      todo->add_source_references();
  ref_gmail2->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/456");
  ref_gmail2->mutable_gmail()->set_subject("Subject 2");

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(true));
  // Notification after adding the todos.
  EXPECT_CALL(mock_page_, OnAutoTodosChanged(Not(IsEmpty())))
      .WillOnce([](const std::vector<context_hub::AutoTodoEntry>& todos) {
        ASSERT_EQ(todos.size(), 1u);
        EXPECT_EQ(todos[0].title, "Test Title");
        EXPECT_EQ(todos[0].description, "Test Description");
        ASSERT_TRUE(todos[0].is_first_party());
        const auto& first_party =
            std::get<context_hub::FirstPartyData>(todos[0].data);
        EXPECT_EQ(first_party.actionable_url,
                  GURL("https://example.com/action2"));
        ASSERT_EQ(first_party.source_references.size(), 2u);
        EXPECT_EQ(first_party.source_references[0].url,
                  GURL("https://mail.google.com/mail/u/0/#inbox/123"));
        EXPECT_EQ(first_party.source_references[0].subject, "Subject 1");
        EXPECT_EQ(first_party.source_references[1].url,
                  GURL("https://mail.google.com/mail/u/0/#inbox/456"));
        EXPECT_EQ(first_party.source_references[1].subject, "Subject 2");
      });
  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  handler_->GenerateFirstPartyAutoTodos(future.GetCallback());
  EXPECT_TRUE(future.Get());
  mock_page_.Flush();
}

TEST_F(ContextHubPageHandlerTest, GenerateFirstPartyAutoTodos_Failure) {
  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::unexpected(
              personal_context::ContextMemoryError::FromExecutionError(
                  personal_context::ContextMemoryError::ExecutionError::
                      kUnknown)))));

  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(true));
  EXPECT_CALL(mock_page_, OnAutoTodosChanged(_)).Times(0);
  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  handler_->GenerateFirstPartyAutoTodos(future.GetCallback());
  EXPECT_FALSE(future.Get());
  mock_page_.Flush();
}

TEST_F(ContextHubPageHandlerTest, GenerateFirstPartyAutoTodos_Empty) {
  personal_context::proto::AutoTodosResponse response;

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(true));
  EXPECT_CALL(mock_page_, OnAutoTodosChanged(IsEmpty()));
  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(false));

  base::test::TestFuture<bool> future;
  handler_->GenerateFirstPartyAutoTodos(future.GetCallback());
  EXPECT_TRUE(future.Get());
  mock_page_.Flush();
}

TEST_F(ContextHubPageHandlerTest, InitialStatePushedIfGenerationInFlight) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(true));
  personal_context::FetchContextCallback saved_callback;
  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce([&](personal_context::proto::ContextMemoryFeature,
                    const google::protobuf::MessageLite&,
                    const personal_context::ContextMemoryRequestOptions&,
                    personal_context::FetchContextCallback callback) {
        saved_callback = std::move(callback);
      });

  // Start generation in the background service directly.
  service->GenerateFirstPartyAutoTodos(base::DoNothing());
  EXPECT_TRUE(service->IsGeneratingFirstPartyAutoTodos());

  // Create a new handler as if opening a new WebUI tab while generation is in
  // flight.
  MockPage new_mock_page;
  EXPECT_CALL(new_mock_page, OnFirstPartyAutoTodosGenerationStateChanged(true));

  auto new_handler = std::make_unique<ContextHubPageHandler>(
      new_mock_page.BindAndGetRemote(),
      mojo::PendingReceiver<browser::context_hub::mojom::PageHandler>(),
      &profile_, nullptr, nullptr);
  new_mock_page.Flush();

  // Complete generation to ensure all observers receive the false event and
  // state is cleaned up.
  EXPECT_CALL(mock_page_, OnFirstPartyAutoTodosGenerationStateChanged(false));
  EXPECT_CALL(new_mock_page,
              OnFirstPartyAutoTodosGenerationStateChanged(false));
  std::move(saved_callback)
      .Run(personal_context::FetchContextResult(base::unexpected(
          personal_context::ContextMemoryError::FromExecutionError(
              personal_context::ContextMemoryError::ExecutionError::
                  kUnknown))));
  mock_page_.Flush();
  new_mock_page.Flush();
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

TEST(ContextHubMojomTraitsTest, GroupTypeSerialization) {
  for (auto group_type :
       {context_hub::ThirdPartyData::GroupType::kNoMatch,
        context_hub::ThirdPartyData::GroupType::kNudgeToClose,
        context_hub::ThirdPartyData::GroupType::kReadingList,
        context_hub::ThirdPartyData::GroupType::kUnfinishedAction,
        context_hub::ThirdPartyData::GroupType::kShoppingCart}) {
    context_hub::ThirdPartyData::GroupType output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<
            browser::context_hub::mojom::AutoTodoGroup>(group_type, output));
    EXPECT_EQ(output, group_type);
  }
}

TEST(ContextHubMojomTraitsTest, GmailReferenceSerialization) {
  context_hub::SourceReference input{
      .url = GURL("https://mail.google.com/mail/u/0/#inbox/123"),
      .subject = "Test Subject",
  };
  context_hub::SourceReference output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::GmailReference>(input, output));
  EXPECT_EQ(output.url, GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(output.subject, "Test Subject");
}

TEST(ContextHubMojomTraitsTest, SourceReferenceSerialization) {
  context_hub::SourceReference input{
      .url = GURL("https://mail.google.com/mail/u/0/#inbox/123"),
      .subject = "Test Subject",
  };
  context_hub::SourceReference output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::SourceReference>(input, output));
  EXPECT_EQ(output.url, GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(output.subject, "Test Subject");
}

TEST(ContextHubMojomTraitsTest, FirstPartyDataSerialization) {
  context_hub::FirstPartyData input;
  input.actionable_url = GURL("https://docs.google.com/doc/123");
  input.source_references.push_back(
      {GURL("https://mail.google.com/mail/u/0/#inbox/123"), "Email Subject 1"});
  input.source_references.push_back(
      {GURL("https://mail.google.com/mail/u/0/#inbox/456"), "Email Subject 2"});

  context_hub::FirstPartyData output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::FirstPartyData>(input, output));

  EXPECT_EQ(output.actionable_url, GURL("https://docs.google.com/doc/123"));
  ASSERT_EQ(output.source_references.size(), 2u);
  EXPECT_EQ(output.source_references[0].url,
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(output.source_references[0].subject, "Email Subject 1");
  EXPECT_EQ(output.source_references[1].url,
            GURL("https://mail.google.com/mail/u/0/#inbox/456"));
  EXPECT_EQ(output.source_references[1].subject, "Email Subject 2");
}

TEST(ContextHubMojomTraitsTest, ThirdPartyDataSerialization) {
  context_hub::ThirdPartyData input;
  input.tab_id = 98765;
  input.last_active_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000);
  input.group_type = context_hub::ThirdPartyData::GroupType::kNudgeToClose;

  context_hub::ThirdPartyData output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              browser::context_hub::mojom::ThirdPartyData>(input, output));

  EXPECT_EQ(output.tab_id, 98765);
  EXPECT_EQ(output.last_active_timestamp,
            base::Time::FromMillisecondsSinceUnixEpoch(1700000000000));
  EXPECT_EQ(output.group_type,
            context_hub::ThirdPartyData::GroupType::kNudgeToClose);
}

TEST(ContextHubMojomTraitsTest, AutoTodoDataSerialization_FirstParty) {
  context_hub::FirstPartyData first_party;
  first_party.actionable_url = GURL("https://docs.google.com/doc/123");
  first_party.source_references.push_back(
      {GURL("https://mail.google.com/mail/u/0/#inbox/123"), "Email Subject"});
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
  EXPECT_EQ(out_first_party.source_references[0].url,
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(out_first_party.source_references[0].subject, "Email Subject");
}

TEST(ContextHubMojomTraitsTest, AutoTodoDataSerialization_ThirdParty) {
  context_hub::ThirdPartyData third_party;
  third_party.tab_id = 54321;
  third_party.last_active_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000);
  third_party.group_type = context_hub::ThirdPartyData::GroupType::kReadingList;
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
  EXPECT_EQ(out_third_party.group_type,
            context_hub::ThirdPartyData::GroupType::kReadingList);
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
  first_party.source_references.push_back(
      {GURL("https://mail.google.com/mail/u/0/#inbox/123"), "Review Request"});
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
  EXPECT_EQ(std::get<context_hub::FirstPartyData>(output.data)
                .source_references[0]
                .url,
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));
  EXPECT_EQ(std::get<context_hub::FirstPartyData>(output.data)
                .source_references[0]
                .subject,
            "Review Request");
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
  third_party.group_type =
      context_hub::ThirdPartyData::GroupType::kUnfinishedAction;
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
  EXPECT_EQ(output.group_type(),
            context_hub::ThirdPartyData::GroupType::kUnfinishedAction);
  EXPECT_EQ(
      std::get<context_hub::ThirdPartyData>(output.data).last_active_timestamp,
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000));
  EXPECT_EQ(std::get<context_hub::ThirdPartyData>(output.data).group_type,
            context_hub::ThirdPartyData::GroupType::kUnfinishedAction);
}

TEST_F(ContextHubPageHandlerTest, GetAutoTodos_Empty) {
  base::test::TestFuture<const std::vector<context_hub::AutoTodoEntry>&,
                         const std::vector<context_hub::AutoTodoEntry>&,
                         base::Time, base::Time>
      future;
  handler_->GetAutoTodos(future.GetCallback());

  auto [first_party, third_party, last_first_party_time,
        last_third_party_time] = future.Take();
  EXPECT_TRUE(first_party.empty());
  EXPECT_TRUE(third_party.empty());
  EXPECT_TRUE(last_first_party_time.is_null());
  EXPECT_TRUE(last_third_party_time.is_null());
}

TEST_F(ContextHubPageHandlerTest, GetAutoTodos_WithTodos) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry fp_entry;
  fp_entry.id = "fp_1";
  fp_entry.title = "First Party Todo";
  fp_entry.description = "FP Description";
  fp_entry.importance_score = 0.9f;
  FirstPartyData fp_data;
  fp_data.actionable_url = GURL("https://example.com/action");
  fp_data.source_references = {
      {GURL("https://mail.google.com/123"), "Email Subject 1"}};
  fp_entry.data = std::move(fp_data);

  base::test::TestFuture<bool> fp_future;
  service->UpdateAutoTodo(std::move(fp_entry), fp_future.GetCallback());
  ASSERT_TRUE(fp_future.Get());

  AutoTodoEntry tp_entry;
  tp_entry.id = "tp_1";
  tp_entry.title = "Third Party Todo";
  tp_entry.description = "TP Description";
  tp_entry.importance_score = 0.5f;
  tp_entry.data = ThirdPartyData{
      .tab_id = 42,
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  base::test::TestFuture<bool> tp_future;
  service->UpdateAutoTodo(std::move(tp_entry), tp_future.GetCallback());
  ASSERT_TRUE(tp_future.Get());

  base::test::TestFuture<const std::vector<context_hub::AutoTodoEntry>&,
                         const std::vector<context_hub::AutoTodoEntry>&,
                         base::Time, base::Time>
      get_future;
  handler_->GetAutoTodos(get_future.GetCallback());

  auto [first_party, third_party, last_first_party_time,
        last_third_party_time] = get_future.Take();
  ASSERT_EQ(first_party.size(), 1u);
  EXPECT_EQ(first_party.at(0).id, "fp_1");
  EXPECT_EQ(first_party.at(0).title, "First Party Todo");
  EXPECT_EQ(first_party.at(0).description, "FP Description");
  EXPECT_EQ(first_party.at(0).importance_score, 0.9f);
  EXPECT_TRUE(first_party.at(0).is_first_party());
  const auto& fp_res_data = std::get<FirstPartyData>(first_party.at(0).data);
  EXPECT_EQ(fp_res_data.actionable_url, GURL("https://example.com/action"));
  ASSERT_EQ(fp_res_data.source_references.size(), 1u);
  EXPECT_EQ(fp_res_data.source_references[0].url,
            GURL("https://mail.google.com/123"));
  EXPECT_EQ(fp_res_data.source_references[0].subject, "Email Subject 1");

  ASSERT_EQ(third_party.size(), 1u);
  EXPECT_EQ(third_party.at(0).id, "tp_1");
  EXPECT_EQ(third_party.at(0).title, "Third Party Todo");
  EXPECT_EQ(third_party.at(0).description, "TP Description");
  EXPECT_EQ(third_party.at(0).importance_score, 0.5f);
  EXPECT_TRUE(third_party.at(0).is_third_party());
  EXPECT_EQ(third_party.at(0).tab_id(), 42);
  EXPECT_EQ(third_party.at(0).group_type(),
            ThirdPartyData::GroupType::kNudgeToClose);
  EXPECT_TRUE(last_first_party_time.is_null());
  EXPECT_TRUE(last_third_party_time.is_null());
}

TEST_F(ContextHubPageHandlerTest, GetAutoTodos_ReturnsLastGenerationTimes) {
  // Before generation, last generation times are null.
  base::test::TestFuture<const std::vector<context_hub::AutoTodoEntry>&,
                         const std::vector<context_hub::AutoTodoEntry>&,
                         base::Time, base::Time>
      initial_future;
  handler_->GetAutoTodos(initial_future.GetCallback());
  auto [initial_fp, initial_tp, initial_fp_time, initial_tp_time] =
      initial_future.Take();
  EXPECT_TRUE(initial_fp_time.is_null());
  EXPECT_TRUE(initial_tp_time.is_null());

  // 1. Generate First-Party Auto Todos via ContextHubService.
  personal_context::proto::AutoTodosResponse response;
  personal_context::proto::AutoTodoItem* todo = response.add_todos();
  todo->set_title("Generated Todo");
  todo->set_description("Generated Description");
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
  base::Time fp_generation_time = base::Time::Now();
  base::test::TestFuture<bool> fp_gen_future;
  handler_->GenerateFirstPartyAutoTodos(fp_gen_future.GetCallback());
  EXPECT_TRUE(fp_gen_future.Get());

  // Verify GetAutoTodos returns the updated 1P timestamp and 3P is still null.
  base::test::TestFuture<const std::vector<context_hub::AutoTodoEntry>&,
                         const std::vector<context_hub::AutoTodoEntry>&,
                         base::Time, base::Time>
      after_fp_future;
  handler_->GetAutoTodos(after_fp_future.GetCallback());
  auto [after_fp, after_tp, after_fp_time, after_tp_time] =
      after_fp_future.Take();
  EXPECT_EQ(after_fp_time, fp_generation_time);
  EXPECT_TRUE(after_tp_time.is_null());
  ASSERT_EQ(after_fp.size(), 1u);
  EXPECT_EQ(after_fp[0].title, "Generated Todo");

#if !BUILDFLAG(IS_ANDROID)
  // 2. Generate Third-Party Auto Todos via ContextHubService.
  EXPECT_CALL(*mock_tab_provider_, GetTabs())
      .WillOnce(testing::Return(std::vector<content::WebContents*>{}));

  // Fast-forward mock time before 3P generation.
  task_environment_.FastForwardBy(base::Hours(2));
  base::Time tp_generation_time = base::Time::Now();

  base::test::TestFuture<bool> tp_gen_future;
  handler_->GenerateTabBasedTodos(tp_gen_future.GetCallback());
  EXPECT_TRUE(tp_gen_future.Get());

  // Verify GetAutoTodos returns both updated timestamps.
  base::test::TestFuture<const std::vector<context_hub::AutoTodoEntry>&,
                         const std::vector<context_hub::AutoTodoEntry>&,
                         base::Time, base::Time>
      after_tp_future;
  handler_->GetAutoTodos(after_tp_future.GetCallback());
  auto [final_fp, final_tp, final_fp_time, final_tp_time] =
      after_tp_future.Take();
  EXPECT_EQ(final_fp_time, fp_generation_time);
  EXPECT_EQ(final_tp_time, tp_generation_time);
#endif
}

TEST_F(ContextHubPageHandlerTest, UpdateAutoTodo_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry todo;
  todo.id = "todo_1";
  todo.title = "Updated Title";
  todo.description = "Updated Description";
  todo.importance_score = 0.75f;
  FirstPartyData fp_data;
  fp_data.actionable_url = GURL("https://example.com/start");
  fp_data.source_references = {
      {GURL("https://mail.google.com/mail/u/0/#inbox/abc"), "ABC Subject"}};
  todo.data = std::move(fp_data);

  base::test::TestFuture<bool> update_future;
  handler_->UpdateAutoTodo(std::move(todo), update_future.GetCallback());
  EXPECT_TRUE(update_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service->GetAutoTodos(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].id, "todo_1");
  EXPECT_EQ(entries[0].title, "Updated Title");
  EXPECT_EQ(entries[0].description, "Updated Description");
  EXPECT_EQ(entries[0].importance_score, 0.75f);
  EXPECT_TRUE(entries[0].is_first_party());
  const auto& stored_fp_data = std::get<FirstPartyData>(entries[0].data);
  EXPECT_EQ(stored_fp_data.actionable_url, GURL("https://example.com/start"));
  ASSERT_EQ(stored_fp_data.source_references.size(), 1u);
  EXPECT_EQ(stored_fp_data.source_references[0].url,
            GURL("https://mail.google.com/mail/u/0/#inbox/abc"));
  EXPECT_EQ(stored_fp_data.source_references[0].subject, "ABC Subject");
}

TEST_F(ContextHubPageHandlerTest, UpdateAutoTodo_ThirdParty_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry todo;
  todo.id = "tp_todo_1";
  todo.title = "Updated Tab Title";
  todo.description = "Updated Tab Description";
  todo.importance_score = 0.85f;
  ThirdPartyData tp_data;
  tp_data.tab_id = 999;
  tp_data.last_active_timestamp =
      base::Time::FromMillisecondsSinceUnixEpoch(1700000000000);
  tp_data.group_type = ThirdPartyData::GroupType::kUnfinishedAction;
  todo.data = std::move(tp_data);

  base::test::TestFuture<bool> update_future;
  handler_->UpdateAutoTodo(std::move(todo), update_future.GetCallback());
  EXPECT_TRUE(update_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service->GetAutoTodos(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].id, "tp_todo_1");
  EXPECT_EQ(entries[0].title, "Updated Tab Title");
  EXPECT_EQ(entries[0].description, "Updated Tab Description");
  EXPECT_EQ(entries[0].importance_score, 0.85f);
  EXPECT_TRUE(entries[0].is_third_party());
  EXPECT_EQ(entries[0].tab_id(), 999);
  EXPECT_EQ(entries[0].group_type(),
            ThirdPartyData::GroupType::kUnfinishedAction);
}

TEST_F(ContextHubPageHandlerTest, ClearFirstPartyAutoTodos) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry fp_entry;
  fp_entry.id = "fp_1";
  fp_entry.title = "Workspace Todo";
  fp_entry.data = FirstPartyData{};

  AutoTodoEntry tp_entry;
  tp_entry.id = "tp_1";
  tp_entry.title = "Browser Todo";
  tp_entry.data = ThirdPartyData{.tab_id = 123};

  base::test::TestFuture<bool> add_future1, add_future2;
  service->UpdateAutoTodo(std::move(fp_entry), add_future1.GetCallback());
  EXPECT_TRUE(add_future1.Get());
  service->UpdateAutoTodo(std::move(tp_entry), add_future2.GetCallback());
  EXPECT_TRUE(add_future2.Get());

  base::test::TestFuture<bool> clear_future;
  handler_->ClearFirstPartyAutoTodos(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service->GetAutoTodos(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].id, "tp_1");
}

TEST_F(ContextHubPageHandlerTest, ClearThirdPartyAutoTodos) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry fp_entry;
  fp_entry.id = "fp_1";
  fp_entry.title = "Workspace Todo";
  fp_entry.data = FirstPartyData{};

  AutoTodoEntry tp_entry;
  tp_entry.id = "tp_1";
  tp_entry.title = "Browser Todo";
  tp_entry.data = ThirdPartyData{.tab_id = 123};

  base::test::TestFuture<bool> add_future1, add_future2;
  service->UpdateAutoTodo(std::move(fp_entry), add_future1.GetCallback());
  EXPECT_TRUE(add_future1.Get());
  service->UpdateAutoTodo(std::move(tp_entry), add_future2.GetCallback());
  EXPECT_TRUE(add_future2.Get());

  base::test::TestFuture<bool> clear_future;
  handler_->ClearThirdPartyAutoTodos(clear_future.GetCallback());
  EXPECT_TRUE(clear_future.Get());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service->GetAutoTodos(get_future.GetCallback());
  auto entries = get_future.Get();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].id, "fp_1");
}

TEST_F(ContextHubPageHandlerTest, OnAutoTodosChanged) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry entry;
  entry.id = "todo_1";
  entry.title = "Test Title";
  entry.description = "Test Description";
  entry.importance_score = 0.8f;

  base::test::TestFuture<std::vector<AutoTodoEntry>> future;
  EXPECT_CALL(mock_page_, OnAutoTodosChanged(_))
      .WillOnce([&future](const std::vector<AutoTodoEntry>& todos) {
        future.SetValue(todos);
      });

  service->UpdateAutoTodo(std::move(entry), base::DoNothing());

  auto todos = future.Take();
  ASSERT_EQ(todos.size(), 1u);
  EXPECT_EQ(todos[0].id, "todo_1");
  EXPECT_EQ(todos[0].title, "Test Title");
  EXPECT_EQ(todos[0].description, "Test Description");
  EXPECT_EQ(todos[0].importance_score, 0.8f);
}

TEST_F(ContextHubPageHandlerTest, GetAutoTodos_FiltersDismissedTodos) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry active_entry;
  active_entry.id = "active_1";
  active_entry.status = AutoTodoEntry::Status::kActive;
  active_entry.data = FirstPartyData{
      .actionable_url = GURL("https://example.com/action"),
  };

  AutoTodoEntry dismissed_entry;
  dismissed_entry.id = "dismissed_1";
  dismissed_entry.status = AutoTodoEntry::Status::kDismissed;
  dismissed_entry.data = FirstPartyData{
      .actionable_url = GURL("https://example.com/action2"),
  };

  AutoTodoEntry completed_entry;
  completed_entry.id = "completed_1";
  completed_entry.status = AutoTodoEntry::Status::kCompleted;
  completed_entry.data = ThirdPartyData{
      .tab_id = 12345,
  };

  // Add Todos to the store.
  base::test::TestFuture<bool> active_future;
  service->UpdateAutoTodo(std::move(active_entry), active_future.GetCallback());
  ASSERT_TRUE(active_future.Get());

  base::test::TestFuture<bool> dismissed_future;
  service->UpdateAutoTodo(std::move(dismissed_entry),
                          dismissed_future.GetCallback());
  ASSERT_TRUE(dismissed_future.Get());

  base::test::TestFuture<bool> completed_future;
  service->UpdateAutoTodo(std::move(completed_entry),
                          completed_future.GetCallback());
  ASSERT_TRUE(completed_future.Get());

  // Verify that GetAutoTodos returns non-dismissed todos to WebUI.
  base::test::TestFuture<const std::vector<context_hub::AutoTodoEntry>&,
                         const std::vector<context_hub::AutoTodoEntry>&,
                         base::Time, base::Time>
      get_future;
  handler_->GetAutoTodos(get_future.GetCallback());

  auto [first_party, third_party, last_first_party_time,
        last_third_party_time] = get_future.Take();
  ASSERT_EQ(first_party.size(), 1u);
  EXPECT_EQ(first_party.at(0).id, "active_1");
  ASSERT_EQ(third_party.size(), 1u);
  EXPECT_EQ(third_party.at(0).id, "completed_1");
  EXPECT_TRUE(last_first_party_time.is_null());
  EXPECT_TRUE(last_third_party_time.is_null());

  // Verify that the dismissed item is still in the cache/store.
  base::test::TestFuture<std::vector<AutoTodoEntry>> service_get_future;
  service->GetAutoTodos(service_get_future.GetCallback());
  auto all_cached_entries = service_get_future.Get();
  EXPECT_EQ(all_cached_entries.size(), 3u);
}

TEST_F(ContextHubPageHandlerTest, OnAutoTodosChanged_FiltersDismissedTodos) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  // Updating the todo to dismissed should notify the page with an empty list.
  AutoTodoEntry dismissed_entry;
  dismissed_entry.id = "todo_1";
  dismissed_entry.status = AutoTodoEntry::Status::kDismissed;
  dismissed_entry.data = FirstPartyData{
      .actionable_url = GURL("https://example.com/action"),
  };

  base::test::TestFuture<std::vector<AutoTodoEntry>> dismissed_notify_future;
  EXPECT_CALL(mock_page_, OnAutoTodosChanged(_))
      .WillOnce([&dismissed_notify_future](
                    const std::vector<AutoTodoEntry>& updated_todos) {
        dismissed_notify_future.SetValue(updated_todos);
      });

  base::test::TestFuture<bool> update_future;
  handler_->UpdateAutoTodo(std::move(dismissed_entry),
                           update_future.GetCallback());
  EXPECT_TRUE(update_future.Get());

  auto updated_todos = dismissed_notify_future.Take();
  EXPECT_TRUE(updated_todos.empty());

  // Verify that the dismissed item is in the cache still, just filtered from
  // the WebUI.
  base::test::TestFuture<std::vector<AutoTodoEntry>> cache_future;
  service->GetAutoTodos(cache_future.GetCallback());
  auto cached_items = cache_future.Get();
  ASSERT_EQ(cached_items.size(), 1u);
  EXPECT_EQ(cached_items[0].id, "todo_1");
  EXPECT_EQ(cached_items[0].status, AutoTodoEntry::Status::kDismissed);
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

  base::test::TestFuture<bool> save_tab_future;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com/tab"),
                      "Tab Title", "Page text"),
      save_tab_future.GetCallback());
  ASSERT_TRUE(save_tab_future.Wait());

  base::test::TestFuture<bool> save_selection_future;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTextSelection,
                      GURL("https://example.com/select"), "Selection Title",
                      "Selected Text Detail"),
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

  base::test::TestFuture<bool> save_tab_future1;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com/tab1"),
                      "Tab Title 1", "Page text 1"),
      save_tab_future1.GetCallback());
  ASSERT_TRUE(save_tab_future1.Wait());

  base::test::TestFuture<bool> save_tab_future2;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com/tab2"),
                      "Tab Title 2", "Page text 2"),
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

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankTags_Empty) {
  base::test::TestFuture<const std::vector<std::string>&> tags_future;
  handler_->GetAllMemoryBankTags(tags_future.GetCallback());
  EXPECT_TRUE(tags_future.Get().empty());
}

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankTags_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  MemoryBankEntry entry1(MemoryBankType::kTab, GURL("https://example.com/tab1"),
                         "Tab Title 1", "Page text 1");
  entry1.tags = {"tag1", "tag2"};
  base::test::TestFuture<bool> save_tab_future1;
  service->SaveMemoryBankEntry(entry1, save_tab_future1.GetCallback());
  ASSERT_TRUE(save_tab_future1.Wait());

  MemoryBankEntry entry2(MemoryBankType::kTab, GURL("https://example.com/tab2"),
                         "Tab Title 2", "Page text 2");
  entry2.tags = {"tag2", "tag3"};
  base::test::TestFuture<bool> save_tab_future2;
  service->SaveMemoryBankEntry(entry2, save_tab_future2.GetCallback());
  ASSERT_TRUE(save_tab_future2.Wait());

  base::test::TestFuture<const std::vector<std::string>&> tags_future;
  handler_->GetAllMemoryBankTags(tags_future.GetCallback());
  EXPECT_THAT(tags_future.Get(),
              testing::UnorderedElementsAre("tag1", "tag2", "tag3"));
}

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankCollections_Empty) {
  base::test::TestFuture<const std::vector<std::string>&> coll_future;
  handler_->GetAllMemoryBankCollections(coll_future.GetCallback());
  EXPECT_TRUE(coll_future.Get().empty());
}

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankCollections_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  MemoryBankEntry entry1(MemoryBankType::kTab, GURL("https://example.com/tab1"),
                         "Tab Title 1", "Page text 1");
  entry1.collection = "Research";
  base::test::TestFuture<bool> save_tab_future1;
  service->SaveMemoryBankEntry(entry1, save_tab_future1.GetCallback());
  ASSERT_TRUE(save_tab_future1.Wait());

  MemoryBankEntry entry2(MemoryBankType::kTab, GURL("https://example.com/tab2"),
                         "Tab Title 2", "Page text 2");
  entry2.collection = "Recipes";
  base::test::TestFuture<bool> save_tab_future2;
  service->SaveMemoryBankEntry(entry2, save_tab_future2.GetCallback());
  ASSERT_TRUE(save_tab_future2.Wait());

  base::test::TestFuture<const std::vector<std::string>&> coll_future;
  handler_->GetAllMemoryBankCollections(coll_future.GetCallback());
  EXPECT_THAT(coll_future.Get(), testing::ElementsAre("Recipes", "Research"));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextHubPageHandlerTest, SwitchToTab) {
  EXPECT_CALL(*mock_tab_provider_, SwitchToTab(42)).Times(1);

  handler_->SwitchToTab(42);
}

TEST_F(ContextHubPageHandlerTest, CloseTab) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  AutoTodoEntry entry;
  entry.id = "tp_1";
  entry.title = "Tab Todo";
  entry.status = AutoTodoEntry::Status::kActive;
  entry.data = ThirdPartyData{
      .tab_id = 42,
      .group_type = ThirdPartyData::GroupType::kNudgeToClose,
  };

  // Add the auto todo entry to cache.
  base::test::TestFuture<bool> bool_future;
  service->UpdateAutoTodo(std::move(entry), bool_future.GetCallback());
  ASSERT_TRUE(bool_future.Get());
  mock_page_.Flush();

  EXPECT_CALL(*mock_tab_provider_, CloseTab(42)).Times(1);

  base::test::TestFuture<std::vector<AutoTodoEntry>> notify_future;
  EXPECT_CALL(mock_page_, OnAutoTodosChanged(_))
      .WillOnce([&notify_future](const std::vector<AutoTodoEntry>& todos) {
        notify_future.SetValue(todos);
      });

  handler_->CloseTab(42);

  // Verify that the WebUI is notified with an empty list.
  auto updated_todos = notify_future.Take();
  EXPECT_TRUE(updated_todos.empty());

  base::test::TestFuture<std::vector<AutoTodoEntry>> get_future;
  service->GetAutoTodos(get_future.GetCallback());
  EXPECT_TRUE(get_future.Get().empty());
}

TEST_F(ContextHubPageHandlerTest, GetTabs_NoTabs) {
  EXPECT_CALL(*mock_tab_provider_, GetUngroupedTabs())
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

  EXPECT_CALL(*mock_tab_provider_, GetUngroupedTabs())
      .WillOnce(testing::Return(raw_test_tabs));

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabInfoPtr>>
      future;
  handler_->GetTabs(future.GetCallback());

  std::vector<browser::context_hub::mojom::TabInfoPtr> tabs = future.Take();
  EXPECT_EQ(tabs.size(), 3u);
}

TEST_F(ContextHubPageHandlerTest, RetrieveAndGroupTabs_NoTabs) {
  EXPECT_CALL(*mock_tab_provider_, GetUngroupedTabs())
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

  EXPECT_CALL(*mock_tab_provider_, GetUngroupedTabs())
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
        group_response->set_text_response("Here are your organized tabs.");
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
  ASSERT_TRUE(llm_response);
  EXPECT_EQ(llm_response->role,
            browser::context_hub::mojom::ChatRole::kAssistant);
  EXPECT_EQ(llm_response->content, "Here are your organized tabs.");
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
  EXPECT_CALL(*mock_tab_provider_, GetUngroupedTabs())
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
  EXPECT_CALL(*mock_tab_provider_, GetUngroupedTabs())
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
  EXPECT_CALL(*mock_tab_provider_, GetUngroupedTabs())
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

TEST_F(ContextHubPageHandlerTest, GenerateTabBasedTodos) {
  std::vector<std::unique_ptr<content::WebContents>> test_tabs;
  std::vector<content::WebContents*> raw_test_tabs;
  auto tab =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  sessions::SessionTabHelper::CreateForWebContents(
      tab.get(), sessions::SessionTabHelper::DelegateLookup());
  raw_test_tabs.push_back(tab.get());
  test_tabs.push_back(std::move(tab));

  EXPECT_CALL(*mock_tab_provider_, GetTabs())
      .WillOnce(testing::Return(raw_test_tabs));

  EXPECT_CALL(mock_page_, OnAutoTodosChanged(_)).Times(0);
  EXPECT_CALL(mock_page_, OnThirdPartyAutoTodosGenerationStateChanged(_))
      .Times(0);

  base::test::TestFuture<bool> future;
  handler_->GenerateTabBasedTodos(future.GetCallback());
  EXPECT_TRUE(future.Get());
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

  base::test::TestFuture<bool> save_future;
  service->SaveMemoryBankEntry(
      MemoryBankEntry(MemoryBankType::kTab, GURL("https://example.com/test"),
                      "Test Page", "Page content"),
      save_future.GetCallback());
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

TEST_F(ContextHubPageHandlerTest, GetConfirmedTabGroups) {
  auto* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(&profile_);
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroup group(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, {},
      /*position=*/std::nullopt, uuid,
      tab_groups::test::GenerateRandomTabGroupID());
  tab_groups::SavedTabGroupTab tab(
      GURL("https://example.com"), u"Example", group.saved_guid(),
      /*position=*/0, /*saved_tab_guid=*/std::nullopt, /*local_tab_id=*/1);
  group.AddTabLocally(tab);
  sync_service->AddGroup(group);

  base::test::TestFuture<std::vector<browser::context_hub::mojom::TabGroupPtr>>
      future;
  handler_->GetConfirmedTabGroups(future.GetCallback());
  auto groups = future.Take();
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ(uuid, groups[0]->saved_guid);
  EXPECT_EQ("Test Group", groups[0]->label);
}

TEST_F(ContextHubPageHandlerTest, RemoveConfirmedTabGroup_InvalidGuid) {
  base::Uuid invalid_uuid;
  base::test::TestFuture<void> future;
  handler_->RemoveConfirmedTabGroup(invalid_uuid, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ContextHubPageHandlerTest, CloseConfirmedTabGroup_InvalidGuid) {
  base::Uuid invalid_uuid;
  base::test::TestFuture<void> future;
  handler_->CloseConfirmedTabGroup(invalid_uuid, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextHubPageHandlerTest, ConfirmAllTabGroups) {
  EXPECT_CALL(*mock_tab_provider_, ConfirmTabGroups(_))
      .WillOnce(testing::Return(true));
  base::test::TestFuture<bool> future;
  handler_->ConfirmAllTabGroups(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ContextHubPageHandlerTest, RemoveConfirmedTabGroup) {
  auto* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(&profile_);
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt, uuid);
  sync_service->AddGroup(group);

  EXPECT_CALL(*mock_tab_provider_, UngroupGroupFromTabstripIfOpen(uuid));

  base::test::TestFuture<void> future;
  handler_->RemoveConfirmedTabGroup(uuid, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ContextHubPageHandlerTest, CloseConfirmedTabGroup) {
  auto* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(&profile_);
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroup group(u"Test Group",
                                  tab_groups::TabGroupColorId::kBlue, {},
                                  /*position=*/std::nullopt, uuid);
  sync_service->AddGroup(group);

  EXPECT_CALL(*mock_tab_provider_, RemoveGroupFromTabstripIfOpen(uuid));

  base::test::TestFuture<void> future;
  handler_->CloseConfirmedTabGroup(uuid, future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(ContextHubPageHandlerTest, RemoveAllConfirmedTabGroups) {
  auto* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(&profile_);
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  tab_groups::SavedTabGroup group(
      u"Test Group", tab_groups::TabGroupColorId::kBlue, {},
      /*position=*/std::nullopt, uuid,
      tab_groups::test::GenerateRandomTabGroupID());
  tab_groups::SavedTabGroupTab tab(
      GURL("https://example.com"), u"Example", group.saved_guid(),
      /*position=*/0, /*saved_tab_guid=*/std::nullopt, /*local_tab_id=*/1);
  group.AddTabLocally(tab);
  sync_service->AddGroup(group);

  EXPECT_CALL(*mock_tab_provider_, UngroupGroupFromTabstripIfOpen(uuid));

  base::test::TestFuture<void> future;
  handler_->RemoveAllConfirmedTabGroups(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ContextHubPageHandlerTest, GetSaveToMemoryBankContext_NotFound) {
  base::test::TestFuture<
      browser::context_hub::mojom::SaveToMemoryBankContextPtr>
      future;
  handler_->GetSaveToMemoryBankContext(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ContextHubPageHandlerTest,
       GetSaveToMemoryBankContext_WithTextSelectionContext) {
  auto* service = ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);
  MemoryBankEntry entry(MemoryBankType::kTextSelection,
                        GURL("https://example.com/test"), "Test Title",
                        "Test Snippet");
  service->SetPendingMemoryBankEntry(std::move(entry));

  base::test::TestFuture<
      browser::context_hub::mojom::SaveToMemoryBankContextPtr>
      future;
  handler_->GetSaveToMemoryBankContext(future.GetCallback());

  browser::context_hub::mojom::SaveToMemoryBankContextPtr context =
      future.Take();
  ASSERT_TRUE(context);
  EXPECT_EQ(context->url, GURL("https://example.com/test"));
  EXPECT_EQ(context->tab_title, "Test Title");
  EXPECT_EQ(context->selected_text, "Test Snippet");
  EXPECT_TRUE(context->is_text_selection);
}

TEST_F(ContextHubPageHandlerTest, GetSaveToMemoryBankContext_WithTabContext) {
  auto* service = ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);
  MemoryBankEntry entry(MemoryBankType::kTab, GURL("https://example.com/test"),
                        "Test Title", "Page Inner Text");
  service->SetPendingMemoryBankEntry(std::move(entry));

  base::test::TestFuture<
      browser::context_hub::mojom::SaveToMemoryBankContextPtr>
      future;
  handler_->GetSaveToMemoryBankContext(future.GetCallback());

  browser::context_hub::mojom::SaveToMemoryBankContextPtr context =
      future.Take();
  ASSERT_TRUE(context);
  EXPECT_EQ(context->url, GURL("https://example.com/test"));
  EXPECT_EQ(context->tab_title, "Test Title");
  EXPECT_FALSE(context->selected_text.has_value());
  EXPECT_FALSE(context->is_text_selection);
}

TEST_F(ContextHubPageHandlerTest,
       GetSaveToMemoryBankContext_TruncatesLargeSnippet) {
  auto* service = ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);
  std::string long_snippet(500, 'a');
  MemoryBankEntry entry(MemoryBankType::kTextSelection,
                        GURL("https://example.com/test"), "Test Title",
                        long_snippet);
  service->SetPendingMemoryBankEntry(std::move(entry));

  base::test::TestFuture<
      browser::context_hub::mojom::SaveToMemoryBankContextPtr>
      future;
  handler_->GetSaveToMemoryBankContext(future.GetCallback());

  browser::context_hub::mojom::SaveToMemoryBankContextPtr context =
      future.Take();
  ASSERT_TRUE(context);
  EXPECT_EQ(context->selected_text->length(), 300u);
  EXPECT_EQ(context->selected_text, std::string(300, 'a'));
}

TEST_F(ContextHubPageHandlerTest, SaveMemoryBankEntry_WithContext) {
  auto* service = ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);
  MemoryBankEntry entry(MemoryBankType::kTextSelection,
                        GURL("https://example.com/test"), "Test Title",
                        "Test Snippet");
  service->SetPendingMemoryBankEntry(std::move(entry));

  auto annotations =
      browser::context_hub::mojom::MemoryBankEntryAnnotations::New();
  annotations->note = "Test Note";
  annotations->collection = "Test Collection";
  annotations->tags = std::vector<std::string>{"tag1"};

  base::test::TestFuture<bool> future;
  handler_->SaveMemoryBankEntry(std::move(annotations), future.GetCallback());
  EXPECT_TRUE(future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> entries_future;
  service->GetAllEntries(entries_future.GetCallback());
  auto entries = entries_future.Take();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].note, "Test Note");
  EXPECT_EQ(entries[0].collection, "Test Collection");
  EXPECT_THAT(entries[0].tags, testing::ElementsAre("tag1"));
}

TEST_F(ContextHubPageHandlerTest, UpdateMemoryBankEntryAnnotations_Success) {
  auto* service = ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);
  MemoryBankEntry entry(MemoryBankType::kTextSelection,
                        GURL("https://example.com/test"), "Test Title",
                        "Test Snippet");
  entry.tags = {"old_tag"};
  entry.note = "Old Note";
  entry.collection = "Old Collection";

  base::test::TestFuture<bool> save_future;
  service->SaveMemoryBankEntry(std::move(entry), save_future.GetCallback());
  EXPECT_TRUE(save_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> entries_future;
  service->GetAllEntries(entries_future.GetCallback());
  auto entries = entries_future.Take();
  ASSERT_EQ(entries.size(), 1u);
  int64_t id = entries[0].id;

  auto new_annotations =
      browser::context_hub::mojom::MemoryBankEntryAnnotations::New();
  new_annotations->note = "New Note";
  new_annotations->collection = "New Collection";
  new_annotations->tags = std::vector<std::string>{"new_tag1", "new_tag2"};

  base::test::TestFuture<bool> update_future;
  handler_->UpdateMemoryBankEntryAnnotations(id, std::move(new_annotations),
                                             update_future.GetCallback());
  EXPECT_TRUE(update_future.Get());

  base::test::TestFuture<std::vector<MemoryBankEntry>> updated_entries_future;
  service->GetAllEntries(updated_entries_future.GetCallback());
  auto updated_entries = updated_entries_future.Take();
  ASSERT_EQ(updated_entries.size(), 1u);
  EXPECT_EQ(updated_entries[0].id, id);
  EXPECT_EQ(updated_entries[0].note, "New Note");
  EXPECT_EQ(updated_entries[0].collection, "New Collection");
  EXPECT_THAT(updated_entries[0].tags,
              testing::ElementsAre("new_tag1", "new_tag2"));
}

TEST_F(ContextHubPageHandlerTest, UpdateMemoryBankEntryAnnotations_NotFound) {
  auto new_annotations =
      browser::context_hub::mojom::MemoryBankEntryAnnotations::New();
  new_annotations->note = "Note";

  base::test::TestFuture<bool> update_future;
  handler_->UpdateMemoryBankEntryAnnotations(999999, std::move(new_annotations),
                                             update_future.GetCallback());
  EXPECT_FALSE(update_future.Get());
}

}  // namespace
}  // namespace context_hub
