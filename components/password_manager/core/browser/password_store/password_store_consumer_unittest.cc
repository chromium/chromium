// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using testing::IsEmpty;

const PasswordStoreBackendError kBackendError =
    PasswordStoreBackendError(PasswordStoreBackendErrorType::kUncategorized);

// Partial mock, used to test the default implementation of the protected
// methods, since those cannot be called from ON_CALL.
class MockPasswordStoreConsumerPartial : public PasswordStoreConsumer {
 public:
  MockPasswordStoreConsumerPartial() = default;
  ~MockPasswordStoreConsumerPartial() override = default;

  MOCK_METHOD(void,
              OnGetPasswordStoreResults,
              (std::vector<std::unique_ptr<PasswordForm>>),
              (override));
};

// Creates a form.
std::unique_ptr<PasswordForm> CreateForm() {
  std::unique_ptr<PasswordForm> form = std::make_unique<PasswordForm>();
  form->username_value = u"username";
  form->password_value = u"password";
  form->url = GURL(u"https://one.example.com/path");
  form->signon_realm = form->url.DeprecatedGetOriginAsURL().spec();
  return form;
}

// These tests use partial mocks. For the code under test, the default
// implementation is used instead of the mocked code.
TEST(PasswordStoreConsumerTest, OnGetPasswordStoreResultsFromIsCalledWithData) {
  MockPasswordStoreConsumer consumer;

  ON_CALL(consumer, OnGetPasswordStoreResultsOrErrorFrom)
      .WillByDefault([&consumer](PasswordStoreInterface* store,
                                 LoginsResultOrError forms_or_error) {
        return consumer
            .PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom(
                store, std::move(forms_or_error));
      });

  std::vector<PasswordForm> forms;
  forms.push_back(*CreateForm());
  std::vector<std::unique_ptr<PasswordForm>> copy_of_forms;
  copy_of_forms.push_back(CreateForm());

  scoped_refptr<MockPasswordStoreInterface> mock_store;
  EXPECT_CALL(consumer, OnGetPasswordStoreResultsFrom(
                            mock_store.get(),
                            UnorderedPasswordFormElementsAre(&copy_of_forms)));
  consumer.OnGetPasswordStoreResultsOrErrorFrom(mock_store.get(),
                                                std::move(forms));
}

TEST(PasswordStoreConsumerTest,
     OnGetPasswordStoreResultsFromCalledWithEmptyOnError) {
  MockPasswordStoreConsumer consumer;

  ON_CALL(consumer, OnGetPasswordStoreResultsOrErrorFrom)
      .WillByDefault([&consumer](PasswordStoreInterface* store,
                                 LoginsResultOrError forms_or_error) {
        return consumer
            .PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom(
                store, std::move(forms_or_error));
      });

  scoped_refptr<MockPasswordStoreInterface> mock_store;
  EXPECT_CALL(consumer,
              OnGetPasswordStoreResultsFrom(mock_store.get(), IsEmpty()));
  consumer.OnGetPasswordStoreResultsOrErrorFrom(mock_store.get(),
                                                kBackendError);
}

TEST(PasswordStoreConsumerTest, OnGetPasswordStoreResultsIsCalledWithData) {
  MockPasswordStoreConsumerPartial consumer;

  std::vector<PasswordForm> forms;
  forms.push_back(*CreateForm());
  std::vector<std::unique_ptr<PasswordForm>> copy_of_forms;
  copy_of_forms.push_back(CreateForm());

  scoped_refptr<MockPasswordStoreInterface> mock_store;
  EXPECT_CALL(consumer, OnGetPasswordStoreResults(
                            UnorderedPasswordFormElementsAre(&copy_of_forms)));
  consumer.OnGetPasswordStoreResultsOrErrorFrom(mock_store.get(),
                                                std::move(forms));
}

TEST(PasswordStoreConsumerTest,
     OnGetPasswordStoreResultsCalledWithEmptyOnError) {
  MockPasswordStoreConsumerPartial consumer;

  scoped_refptr<MockPasswordStoreInterface> mock_store;
  EXPECT_CALL(consumer, OnGetPasswordStoreResults(IsEmpty()));
  consumer.OnGetPasswordStoreResultsOrErrorFrom(mock_store.get(),
                                                kBackendError);
}

}  // namespace password_manager
