// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"

#include "base/callback.h"
#include "base/test/task_environment.h"

#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

// Creates a form.
PasswordForm CreateForm(base::StringPiece16 username,
                        base::StringPiece16 password) {
  PasswordForm form;
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  return form;
}

std::vector<std::unique_ptr<PasswordForm>> MakeCopy(
    const std::vector<PasswordForm>& forms) {
  std::vector<std::unique_ptr<PasswordForm>> copy;
  for (const auto& form : forms)
    copy.push_back(std::make_unique<PasswordForm>(form));
  return copy;
}

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MockPasswordStoreConsumer() = default;

  MOCK_METHOD(void,
              OnGetPasswordStoreResultsConstRef,
              (const std::vector<PasswordForm>&),
              ());

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    std::vector<PasswordForm> forms;
    base::ranges::transform(results, std::back_inserter(forms),
                            [](auto& result) { return std::move(*result); });
    OnGetPasswordStoreResultsConstRef(forms);
  }
};

}  // namespace

class GetLoginsWithAffiliationsRequestHandlerTest : public testing::Test {
 public:
  GetLoginsWithAffiliationsRequestHandlerTest()
      : store_(new MockPasswordStoreInterface()),
        consumer_(new MockPasswordStoreConsumer()) {}

  MockPasswordStoreConsumer* consumer() { return consumer_.get(); }
  MockPasswordStoreInterface* store() { return store_.get(); }
  scoped_refptr<GetLoginsWithAffiliationsRequestHandler> MakeRequestHandler() {
    return base::MakeRefCounted<GetLoginsWithAffiliationsRequestHandler>(
        consumer_->GetWeakPtr(), store_.get());
  }

  void TearDown() override { store()->ShutdownOnUIThread(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<MockPasswordStoreInterface> store_;
  std::unique_ptr<MockPasswordStoreConsumer> consumer_;
};

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, LoginsReceivedFirst) {
  std::vector<PasswordForm> forms = {
      CreateForm(u"username1", u"password"),
      CreateForm(u"username2", u"password"),
  };
  PasswordForm affiliated_form = CreateForm(u"username3", u"password");

  std::vector<PasswordForm> expected_forms = forms;
  expected_forms.push_back(affiliated_form);
  expected_forms.back().is_affiliation_based_match = true;

  auto handler = MakeRequestHandler();
  handler->LoginsForFormClosure().Run(MakeCopy(forms));

  EXPECT_CALL(*consumer(),
              OnGetPasswordStoreResultsConstRef(
                  testing::UnorderedElementsAreArray(expected_forms)));

  handler->AffiliatedLoginsClosure().Run(MakeCopy({affiliated_form}));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest,
       AffiliatedLoginsReceivedFirst) {
  std::vector<PasswordForm> forms = {
      CreateForm(u"username1", u"password"),
      CreateForm(u"username2", u"password"),
  };
  PasswordForm affiliated_form = CreateForm(u"username3", u"password");
  std::vector<PasswordForm> expected_forms = forms;
  expected_forms.push_back(affiliated_form);
  expected_forms.back().is_affiliation_based_match = true;

  auto handler = MakeRequestHandler();
  handler->AffiliatedLoginsClosure().Run(MakeCopy({affiliated_form}));

  EXPECT_CALL(*consumer(),
              OnGetPasswordStoreResultsConstRef(
                  testing::UnorderedElementsAreArray(expected_forms)));

  handler->LoginsForFormClosure().Run(MakeCopy(forms));
}

TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, ConsumerNotNotified) {
  auto handler = MakeRequestHandler();
  std::vector<PasswordForm> forms = {
      CreateForm(u"username1", u"password"),
      CreateForm(u"username2", u"password"),
  };
  PasswordForm affiliated_form = CreateForm(u"username3", u"password");

  consumer()->CancelAllRequests();
  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsConstRef).Times(0);

  handler->AffiliatedLoginsClosure().Run(MakeCopy({affiliated_form}));
  handler->LoginsForFormClosure().Run(MakeCopy(forms));
}

// Tests that handler lives out of scope it was declared.
TEST_F(GetLoginsWithAffiliationsRequestHandlerTest, LivesLongerThanScope) {
  base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
      forms_callback;
  base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
      affiliated_callback;

  {
    auto handler = MakeRequestHandler();
    forms_callback = handler->LoginsForFormClosure();
    affiliated_callback = handler->LoginsForFormClosure();
  };

  std::vector<PasswordForm> forms = {
      CreateForm(u"username1", u"password"),
      CreateForm(u"username2", u"password"),
  };

  std::move(forms_callback).Run(MakeCopy(forms));

  EXPECT_CALL(*consumer(), OnGetPasswordStoreResultsConstRef(
                               testing::UnorderedElementsAreArray(forms)));

  std::move(affiliated_callback).Run({});
}

}  // namespace password_manager
