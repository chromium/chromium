// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/submitted_context_decorator.h"

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/lens/contextual_input.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace contextual_tasks {

class SubmittedContextDecoratorTest : public testing::Test {
 public:
  SubmittedContextDecoratorTest() {
    contextual_search::ContextualSearchService::RegisterProfilePrefs(
        pref_service_.registry());
  }
  ~SubmittedContextDecoratorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SubmittedContextDecoratorTest, Construction) {
  // Verifies that the decorator can be constructed and called with a null
  // `ContextDecorationParams`, and that it calls the callback with the original
  // context.
  SubmittedContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);
  auto* context_ptr = context.get();

  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), nullptr,
      base::BindOnce(
          [](ContextualTaskContext* expected_context,
             base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            EXPECT_EQ(context.get(), expected_context);
            std::move(quit_closure).Run();
          },
          context_ptr, run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(SubmittedContextDecoratorTest, DecorateWithContextualSearchData) {
  // Set up the Contextual Search service and a mock controller.
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      /*tab_validator=*/nullptr, base::DoNothing());
  auto mock_controller = std::make_unique<
      contextual_search::MockContextualSearchContextController>();
  auto* mock_controller_ptr = mock_controller.get();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);
  // Check the search content sharing settings to notify the session handle
  // that the client is properly checking the pref value.
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  // Add a tab context to the session, which will produce a token.
  base::UnguessableToken token = session_handle->CreateContextToken();

  // Move the token to the submitted state.
  session_handle->CreateClientToAimRequest(
      std::make_unique<contextual_search::ContextualSearchContextController::
                           CreateClientToAimRequestInfo>());

  // Add a second tab context that will remain in the uploaded state.
  session_handle->CreateContextToken();

  // Mock the controller to return valid file info for the token.
  contextual_search::FileInfo file_info;
  file_info.tab_url = GURL("https://example.com/");
  file_info.tab_title = "Test Title";
  file_info.tab_session_id = SessionID::FromSerializedValue(123);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token))
      .WillOnce(testing::Return(&file_info));

  // Note: we do not expect GetFileInfo(token2) because it is uploaded, not
  // submitted.

  // Set up the decoration params with the session handle.
  ContextDecorationParams params;
  params.contextual_search_session_handle = session_handle->AsWeakPtr();

  // Decorate the context.
  SubmittedContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  // Run the decoration and verify the context is updated as expected.
  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), &params,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            ASSERT_EQ(1u, context->GetUrlAttachments().size());
            auto& attachment = context->GetMutableUrlAttachmentsForTesting()[0];
            EXPECT_EQ("https://example.com/", attachment.GetURL());
            EXPECT_EQ(u"Test Title", attachment.GetTitle());
            EXPECT_EQ(SessionID::FromSerializedValue(123),
                      attachment.GetTabSessionId());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(SubmittedContextDecoratorTest, DecorateWithNullSessionHandle) {
  // Set up decoration params with a null session handle.
  ContextDecorationParams params;

  // Decorate the context.
  SubmittedContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  // Run the decoration and verify that no attachments are added.
  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), &params,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            EXPECT_TRUE(context->GetUrlAttachments().empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(SubmittedContextDecoratorTest, DecorateWithNoContextTokens) {
  // Set up a session handle with no context tokens.
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      /*tab_validator=*/nullptr, base::DoNothing());
  auto mock_controller = std::make_unique<
      contextual_search::MockContextualSearchContextController>();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);
  // Check the search content sharing settings to notify the session handle
  // that the client is properly checking the pref value.
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  // Set up decoration params with the session handle.
  ContextDecorationParams params;
  params.contextual_search_session_handle = session_handle->AsWeakPtr();

  // Decorate the context.
  SubmittedContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  // Run the decoration and verify that no attachments are added.
  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), &params,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            EXPECT_TRUE(context->GetUrlAttachments().empty());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(SubmittedContextDecoratorTest, DecorateWithIncompleteData) {
  // Set up the service and session handle.
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      /*tab_validator=*/nullptr, base::DoNothing());
  auto mock_controller = std::make_unique<
      contextual_search::MockContextualSearchContextController>();
  auto* mock_controller_ptr = mock_controller.get();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);
  // Check the search content sharing settings to notify the session handle
  // that the client is properly checking the pref value.
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  // Add three tokens: one valid, one with no URL, and one that will have a
  // null FileInfo.
  base::UnguessableToken valid_token = session_handle->CreateContextToken();

  base::UnguessableToken no_url_token = session_handle->CreateContextToken();

  base::UnguessableToken null_file_info_token =
      session_handle->CreateContextToken();

  // Move the tokens to the submitted state.
  session_handle->CreateClientToAimRequest(
      std::make_unique<contextual_search::ContextualSearchContextController::
                           CreateClientToAimRequestInfo>());

  // Mock the controller to return appropriate data for each token.
  contextual_search::FileInfo valid_file_info;
  valid_file_info.tab_url = GURL("https://example.com/");
  valid_file_info.tab_title = "Test Title";
  valid_file_info.tab_session_id = SessionID::FromSerializedValue(123);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(valid_token))
      .WillOnce(testing::Return(&valid_file_info));

  contextual_search::FileInfo no_url_file_info;
  no_url_file_info.tab_title = "No URL Title";
  no_url_file_info.tab_session_id = SessionID::FromSerializedValue(124);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(no_url_token))
      .WillOnce(testing::Return(&no_url_file_info));

  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(null_file_info_token))
      .WillOnce(testing::Return(nullptr));

  // Set up the decoration params.
  ContextDecorationParams params;
  params.contextual_search_session_handle = session_handle->AsWeakPtr();

  // Decorate the context.
  SubmittedContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  // Run the decoration and verify that only the single valid attachment was
  // created.
  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), &params,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            ASSERT_EQ(1u, context->GetUrlAttachments().size());
            auto& attachment = context->GetMutableUrlAttachmentsForTesting()[0];
            EXPECT_EQ("https://example.com/", attachment.GetURL());
            EXPECT_EQ(u"Test Title", attachment.GetTitle());
            EXPECT_EQ(SessionID::FromSerializedValue(123),
                      attachment.GetTabSessionId());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(SubmittedContextDecoratorTest,
       DecorateWithPersistedSubmittedTabsAfterClearSubmittedTokens) {
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      /*tab_validator=*/nullptr, base::DoNothing());
  auto mock_controller = std::make_unique<
      contextual_search::MockContextualSearchContextController>();
  auto* mock_controller_ptr = mock_controller.get();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  base::UnguessableToken token = session_handle->CreateContextToken();

  contextual_search::FileInfo file_info;
  file_info.file_token = token;
  file_info.tab_url = GURL("https://example.com/");
  file_info.tab_title = "Persisted Tab Title";
  file_info.tab_session_id = SessionID::FromSerializedValue(123);
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(456);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&file_info));

  session_handle->CreateClientToAimRequest(
      std::make_unique<contextual_search::ContextualSearchContextController::
                           CreateClientToAimRequestInfo>());

  // Simulate AIM acknowledging the turn context library and clearing
  // `submitted_context_tokens_`, leaving the tab in `tab_context_.attached`.
  session_handle->ClearSubmittedContextTokens();
  ASSERT_TRUE(session_handle->GetSubmittedContextTokens().empty());

  ContextDecorationParams params;
  params.contextual_search_session_handle = session_handle->AsWeakPtr();

  SubmittedContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), &params,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            ASSERT_EQ(1u, context->GetUrlAttachments().size());
            auto& attachment = context->GetMutableUrlAttachmentsForTesting()[0];
            EXPECT_EQ("https://example.com/", attachment.GetURL());
            EXPECT_EQ(u"Persisted Tab Title", attachment.GetTitle());
            EXPECT_EQ(SessionID::FromSerializedValue(123),
                      attachment.GetTabSessionId());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(SubmittedContextDecoratorTest,
       DecorateSkipsDeselectedAndUnsubmittedAttachedTabsAndDeduplicates) {
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "",
      /*tab_validator=*/nullptr, base::DoNothing());
  auto mock_controller = std::make_unique<
      contextual_search::MockContextualSearchContextController>();
  auto* mock_controller_ptr = mock_controller.get();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);
  session_handle->CheckSearchContentSharingSettings(&pref_service_);

  // Active submitted tab present in both `submitted_context_tokens_` and
  // `tab_context_.attached` (must be decorated once, not duplicated).
  base::UnguessableToken active_token = session_handle->CreateContextToken();
  contextual_search::FileInfo active_info;
  active_info.file_token = active_token;
  active_info.tab_url = GURL("https://example.com/active");
  active_info.tab_title = "Active Tab";
  active_info.tab_session_id = SessionID::FromSerializedValue(10);
  active_info.request_id.emplace();
  active_info.request_id->set_context_id(100);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(active_token))
      .WillRepeatedly(testing::Return(&active_info));

  // Deselected submitted tab present in `tab_context_.attached` (must be
  // skipped).
  base::UnguessableToken deselected_token =
      session_handle->CreateContextToken();
  contextual_search::FileInfo deselected_info;
  deselected_info.file_token = deselected_token;
  deselected_info.tab_url = GURL("https://example.com/deselected");
  deselected_info.tab_title = "Deselected Tab";
  deselected_info.tab_session_id = SessionID::FromSerializedValue(20);
  deselected_info.request_id.emplace();
  deselected_info.request_id->set_context_id(200);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(deselected_token))
      .WillRepeatedly(testing::Return(&deselected_info));

  session_handle->CreateClientToAimRequest(
      std::make_unique<contextual_search::ContextualSearchContextController::
                           CreateClientToAimRequestInfo>());

  // Clear `submitted_context_tokens_` for `deselected_token` by keeping only
  // `active_token` in `submitted_context_tokens_`, and mark tab 20 deselected.
  session_handle->SetSubmittedContextTokens({active_token});
  session_handle->set_deselected_tabs_urls(
      {{SessionID::FromSerializedValue(20),
        {GURL("https://example.com/deselected"), "Deselected Tab"}}});

  // Add an unsubmitted delayed tab to `tab_context_.attached` (must be
  // skipped).
  base::UnguessableToken delayed_token = session_handle->CreateContextToken();
  session_handle->AddDelayedTabContext(
      delayed_token, 30, GURL("https://example.com/delayed"), "Delayed Tab");
  ASSERT_EQ(3u, session_handle->GetTabContextState().attached.size());

  ContextDecorationParams params;
  params.contextual_search_session_handle = session_handle->AsWeakPtr();

  SubmittedContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), &params,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            ASSERT_EQ(1u, context->GetUrlAttachments().size());
            auto& attachment = context->GetMutableUrlAttachmentsForTesting()[0];
            EXPECT_EQ("https://example.com/active", attachment.GetURL());
            EXPECT_EQ(u"Active Tab", attachment.GetTitle());
            EXPECT_EQ(SessionID::FromSerializedValue(10),
                      attachment.GetTabSessionId());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace contextual_tasks
