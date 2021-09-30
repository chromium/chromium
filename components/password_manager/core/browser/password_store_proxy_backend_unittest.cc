// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_proxy_backend.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "components/password_manager/core/browser/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Pointer;
using ::testing::StrictMock;
using ::testing::WithArg;
using Type = PasswordStoreChange::Type;

PasswordForm CreateTestForm() {
  PasswordForm form;
  form.username_value = u"Todd Tester";
  form.password_value = u"S3cr3t";
  form.url = GURL(u"https://example.com");
  form.is_public_suffix_match = false;
  form.is_affiliation_based_match = false;
  return form;
}

std::vector<std::unique_ptr<PasswordForm>> CreateTestLogins() {
  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(CreateEntry("Todd Tester", "S3cr3t",
                              GURL(u"https://example.com"),
                              /*is_psl_match=*/false,
                              /*is_affiliation_based_match=*/false));
  forms.push_back(CreateEntry("Marcus McSpartanGregor", "S0m3th1ngCr34t1v3",
                              GURL(u"https://m.example.com"),
                              /*is_psl_match=*/true,
                              /*is_affiliation_based_match=*/false));
  return forms;
}

bool FilterNoUrl(const GURL& gurl) {
  return true;
}

}  // namespace

class PasswordStoreProxyBackendTest : public testing::Test {
 protected:
  PasswordStoreProxyBackendTest() {
    proxy_backend_ = std::make_unique<PasswordStoreProxyBackend>(
        CreateMainBackend(), CreateShadowBackend());
  }

  void TearDown() override {
    EXPECT_CALL(*shadow_backend_, Shutdown(_));
    EXPECT_CALL(*main_backend_, Shutdown(_));
    shadow_backend_ = nullptr;
    main_backend_ = nullptr;
    PasswordStoreBackend* backend = proxy_backend_.get();  // Will be destroyed.
    backend->Shutdown(base::DoNothing());
    proxy_backend_.reset();
  }

  PasswordStoreBackend& proxy_backend() { return *proxy_backend_; }
  MockPasswordStoreBackend* main_backend() { return main_backend_; }
  MockPasswordStoreBackend* shadow_backend() { return shadow_backend_; }

 private:
  std::unique_ptr<PasswordStoreBackend> CreateMainBackend() {
    auto unique_backend =
        std::make_unique<StrictMock<MockPasswordStoreBackend>>();
    main_backend_ = unique_backend.get();
    return unique_backend;
  }

  std::unique_ptr<PasswordStoreBackend> CreateShadowBackend() {
    auto unique_backend =
        std::make_unique<StrictMock<MockPasswordStoreBackend>>();
    shadow_backend_ = unique_backend.get();
    return unique_backend;
  }

  std::unique_ptr<PasswordStoreProxyBackend> proxy_backend_;
  StrictMock<MockPasswordStoreBackend>* main_backend_;
  StrictMock<MockPasswordStoreBackend>* shadow_backend_;
};

TEST_F(PasswordStoreProxyBackendTest, CallCompletionCallbackAfterInit) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // Both backends need to be invoked for a successful completion call.
  EXPECT_CALL(*main_backend(), InitBackend)
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(*shadow_backend(), InitBackend)
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(completion_callback, Run(true));
  proxy_backend().InitBackend(base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
}

TEST_F(PasswordStoreProxyBackendTest, CallCompletionWithFailureForAnyError) {
  base::MockCallback<base::OnceCallback<void(bool)>> completion_callback;

  // If one backend fails to initialize, the result of the second is irrelevant.
  EXPECT_CALL(*main_backend(), InitBackend)
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(false);
          })));
  EXPECT_CALL(*shadow_backend(), InitBackend)
      .Times(AtMost(1))
      .WillOnce(
          WithArg<2>(Invoke([](base::OnceCallback<void(bool)> reply) -> void {
            std::move(reply).Run(true);
          })));
  EXPECT_CALL(completion_callback, Run(false));
  proxy_backend().InitBackend(base::DoNothing(), base::DoNothing(),
                              completion_callback.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToGetAllLoginsAsync) {
  base::MockCallback<LoginsReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));
  EXPECT_CALL(*main_backend(), GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  proxy_backend().GetAllLoginsAsync(mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToGetAutofillableLoginsAsync) {
  base::MockCallback<LoginsReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));
  EXPECT_CALL(*main_backend(), GetAutofillableLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  proxy_backend().GetAutofillableLoginsAsync(mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToFillMatchingLoginsAsync) {
  base::MockCallback<LoginsReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_logins)));
  EXPECT_CALL(*main_backend(), FillMatchingLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsReply reply) -> void {
        std::move(reply).Run(CreateTestLogins());
      })));
  proxy_backend().FillMatchingLoginsAsync(mock_reply.Get(),
                                          /*include_psl=*/false,
                                          std::vector<PasswordFormDigest>());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToAddLoginAsync) {
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::ADD, form));
  EXPECT_CALL(mock_reply, Run(Eq(change_list)));
  EXPECT_CALL(*main_backend(), AddLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordStoreChangeListReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().AddLoginAsync(form, mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToUpdateLoginAsync) {
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::UPDATE, form));
  EXPECT_CALL(mock_reply, Run(Eq(change_list)));
  EXPECT_CALL(*main_backend(), UpdateLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordStoreChangeListReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().UpdateLoginAsync(form, mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToRemoveLoginAsync) {
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  PasswordForm form = CreateTestForm();
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, form));
  EXPECT_CALL(mock_reply, Run(Eq(change_list)));
  EXPECT_CALL(*main_backend(), RemoveLoginAsync(Eq(form), _))
      .WillOnce(WithArg<1>(
          Invoke([&change_list](PasswordStoreChangeListReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().RemoveLoginAsync(form, mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToRemoveLoginsByURLAndTimeAsync) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, CreateTestForm()));
  EXPECT_CALL(mock_reply, Run(Eq(change_list)));
  EXPECT_CALL(*main_backend(),
              RemoveLoginsByURLAndTimeAsync(_, Eq(kStart), Eq(kEnd), _, _))
      .WillOnce(WithArg<4>(
          Invoke([&change_list](PasswordStoreChangeListReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().RemoveLoginsByURLAndTimeAsync(
      base::BindRepeating(&FilterNoUrl), kStart, kEnd, base::DoNothing(),
      mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToRemoveLoginsCreatedBetweenAsync) {
  base::Time kStart = base::Time::FromTimeT(111111);
  base::Time kEnd = base::Time::FromTimeT(22222222);
  base::MockCallback<PasswordStoreChangeListReply> mock_reply;
  PasswordStoreChangeList change_list;
  change_list.push_back(PasswordStoreChange(Type::REMOVE, CreateTestForm()));
  EXPECT_CALL(mock_reply, Run(Eq(change_list)));
  EXPECT_CALL(*main_backend(),
              RemoveLoginsCreatedBetweenAsync(Eq(kStart), Eq(kEnd), _))
      .WillOnce(WithArg<2>(
          Invoke([&change_list](PasswordStoreChangeListReply reply) -> void {
            std::move(reply).Run(change_list);
          })));
  proxy_backend().RemoveLoginsCreatedBetweenAsync(kStart, kEnd,
                                                  mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToDisableAutoSignInForOriginsAsync) {
  base::MockCallback<base::OnceClosure> mock_reply;
  EXPECT_CALL(mock_reply, Run);
  EXPECT_CALL(*main_backend(), DisableAutoSignInForOriginsAsync)
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceClosure reply) { std::move(reply).Run(); })));
  proxy_backend().DisableAutoSignInForOriginsAsync(
      base::BindRepeating(&FilterNoUrl), mock_reply.Get());
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToGetSmartBubbleStatsStore) {
  EXPECT_CALL(*main_backend(), GetSmartBubbleStatsStore);
  proxy_backend().GetSmartBubbleStatsStore();
}

TEST_F(PasswordStoreProxyBackendTest, UseMainBackendToGetFieldInfoStore) {
  EXPECT_CALL(*main_backend(), GetFieldInfoStore);
  proxy_backend().GetFieldInfoStore();
}

TEST_F(PasswordStoreProxyBackendTest,
       UseMainBackendToCreateSyncControllerDelegateFactory) {
  EXPECT_CALL(*main_backend(), CreateSyncControllerDelegateFactory);
  proxy_backend().CreateSyncControllerDelegateFactory();
}

}  // namespace password_manager
