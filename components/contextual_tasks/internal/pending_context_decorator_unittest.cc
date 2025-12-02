// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/pending_context_decorator.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/lens/contextual_input.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class MockContextualSearchContextController
    : public contextual_search::ContextualSearchContextController {
 public:
  MockContextualSearchContextController() = default;
  ~MockContextualSearchContextController() override = default;

  MOCK_METHOD(void, InitializeIfNeeded, (), (override));
  MOCK_METHOD(GURL,
              CreateSearchUrl,
              (std::unique_ptr<CreateSearchUrlRequestInfo>),
              (override));
  MOCK_METHOD(lens::ClientToAimMessage,
              CreateClientToAimRequest,
              (std::unique_ptr<CreateClientToAimRequestInfo>),
              (override));
  MOCK_METHOD(void, AddObserver, (FileUploadStatusObserver*), (override));
  MOCK_METHOD(void, RemoveObserver, (FileUploadStatusObserver*), (override));
  MOCK_METHOD(void,
              StartFileUploadFlow,
              (const base::UnguessableToken&,
               std::unique_ptr<lens::ContextualInputData>,
               std::optional<lens::ImageEncodingOptions>),
              (override));
  MOCK_METHOD(bool, DeleteFile, (const base::UnguessableToken&), (override));
  MOCK_METHOD(void, ClearFiles, (), (override));
  MOCK_METHOD(std::unique_ptr<lens::proto::LensOverlaySuggestInputs>,
              CreateSuggestInputs,
              (const std::vector<base::UnguessableToken>&),
              (override));
  MOCK_METHOD(const contextual_search::FileInfo*,
              GetFileInfo,
              (const base::UnguessableToken&),
              (override));
  MOCK_METHOD(std::vector<const contextual_search::FileInfo*>,
              GetFileInfoList,
              (),
              (override));
};

class PendingContextDecoratorTest : public testing::Test {
 public:
  PendingContextDecoratorTest() = default;
  ~PendingContextDecoratorTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PendingContextDecoratorTest, Construction) {
  // Verifies that the decorator can be constructed and called with a null
  // `ContextDecorationParams`, and that it calls the callback with the original
  // context.
  PendingContextDecorator decorator;
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

TEST_F(PendingContextDecoratorTest, DecorateWithContextualSearchData) {
  // Set up the Contextual Search service and a mock controller.
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "");
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto* mock_controller_ptr = mock_controller.get();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);

  // Add a tab context to the session, which will produce a token.
  base::UnguessableToken token;
  session_handle->AddTabContext(
      123, base::BindOnce(
               [](base::UnguessableToken* out_token,
                  const base::UnguessableToken& new_token) {
                 *out_token = new_token;
               },
               &token));

  // Move the token to the submitted state.
  session_handle->CreateClientToAimRequest(
      std::make_unique<contextual_search::ContextualSearchContextController::
                           CreateClientToAimRequestInfo>());

  // Add a second tab context that will remain in the uploaded state.
  base::UnguessableToken token2;
  session_handle->AddTabContext(
      456, base::BindOnce(
               [](base::UnguessableToken* out_token,
                  const base::UnguessableToken& new_token) {
                 *out_token = new_token;
               },
               &token2));

  // Mock the controller to return valid file info for the token.
  contextual_search::FileInfo file_info;
  file_info.tab_url = GURL("https://example.com/");
  file_info.tab_title = "Test Title";
  file_info.tab_session_id = SessionID::FromSerializedValue(123);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token))
      .WillOnce(testing::Return(&file_info));

  contextual_search::FileInfo file_info2;
  file_info2.tab_url = GURL("https://example2.com/");
  file_info2.tab_title = "Test Title 2";
  file_info2.tab_session_id = SessionID::FromSerializedValue(456);
  EXPECT_CALL(*mock_controller_ptr, GetFileInfo(token2))
      .WillOnce(testing::Return(&file_info2));

  // Set up the decoration params with the session handle.
  ContextDecorationParams params;
  params.contextual_search_session_handle = session_handle->AsWeakPtr();

  // Decorate the context.
  PendingContextDecorator decorator;
  ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context = std::make_unique<ContextualTaskContext>(task);

  // Run the decoration and verify the context is updated as expected.
  base::RunLoop run_loop;
  decorator.DecorateContext(
      std::move(context), &params,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<ContextualTaskContext> context) {
            ASSERT_EQ(2u, context->GetUrlAttachments().size());
            auto& attachment = context->GetMutableUrlAttachmentsForTesting()[0];
            EXPECT_EQ("https://example2.com/", attachment.GetURL());
            EXPECT_EQ(u"Test Title 2", attachment.GetTitle());
            EXPECT_EQ(SessionID::FromSerializedValue(456),
                      attachment.GetTabSessionId());
            auto& attachment2 =
                context->GetMutableUrlAttachmentsForTesting()[1];
            EXPECT_EQ("https://example.com/", attachment2.GetURL());
            EXPECT_EQ(u"Test Title", attachment2.GetTitle());
            EXPECT_EQ(SessionID::FromSerializedValue(123),
                      attachment2.GetTabSessionId());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PendingContextDecoratorTest, DecorateWithNullSessionHandle) {
  // Set up decoration params with a null session handle.
  ContextDecorationParams params;

  // Decorate the context.
  PendingContextDecorator decorator;
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

TEST_F(PendingContextDecoratorTest, DecorateWithNoContextTokens) {
  // Set up a session handle with no context tokens.
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "");
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);

  // Set up decoration params with the session handle.
  ContextDecorationParams params;
  params.contextual_search_session_handle = session_handle->AsWeakPtr();

  // Decorate the context.
  PendingContextDecorator decorator;
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

TEST_F(PendingContextDecoratorTest, DecorateWithIncompleteData) {
  // Set up the service and session handle.
  contextual_search::ContextualSearchService service(
      nullptr, nullptr, nullptr, nullptr, version_info::Channel::UNKNOWN, "");
  auto mock_controller =
      std::make_unique<MockContextualSearchContextController>();
  auto* mock_controller_ptr = mock_controller.get();
  auto session_handle =
      service.CreateSessionForTesting(std::move(mock_controller), nullptr);

  // Add three tokens: one valid, one with no URL, and one that will have a
  // null FileInfo.
  base::UnguessableToken valid_token;
  session_handle->AddTabContext(
      123, base::BindOnce(
               [](base::UnguessableToken* out_token,
                  const base::UnguessableToken& new_token) {
                 *out_token = new_token;
               },
               &valid_token));

  base::UnguessableToken no_url_token;
  session_handle->AddTabContext(
      124, base::BindOnce(
               [](base::UnguessableToken* out_token,
                  const base::UnguessableToken& new_token) {
                 *out_token = new_token;
               },
               &no_url_token));

  base::UnguessableToken null_file_info_token;
  session_handle->AddTabContext(
      125, base::BindOnce(
               [](base::UnguessableToken* out_token,
                  const base::UnguessableToken& new_token) {
                 *out_token = new_token;
               },
               &null_file_info_token));

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
  PendingContextDecorator decorator;
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

}  // namespace contextual_tasks
