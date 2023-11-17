// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_CONSUMER_H_

#include <memory>

#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

struct PasswordForm;

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MockPasswordStoreConsumer();
  ~MockPasswordStoreConsumer() override;

  MOCK_METHOD(void,
              OnGetPasswordStoreResults,
              (std::vector<std::unique_ptr<PasswordForm>>),
              (override));
  MOCK_METHOD(void,
              OnGetPasswordStoreResultsFrom,
              (PasswordStoreInterface*,
               std::vector<std::unique_ptr<PasswordForm>>),
              (override));

  MOCK_METHOD(void,
              OnGetPasswordStoreResultsOrErrorFrom,
              (PasswordStoreInterface*, LoginsResultOrError),
              (override));

  base::WeakPtr<PasswordStoreConsumer> GetWeakPtr();

  void CancelAllRequests();

 private:
  base::WeakPtrFactory<MockPasswordStoreConsumer> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_CONSUMER_H_
