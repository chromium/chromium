// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_migration_decorator.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;

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
  forms[0]->in_store = PasswordForm::Store::kProfileStore;
  forms[1]->in_store = PasswordForm::Store::kProfileStore;
  return forms;
}

}  // namespace

class PasswordStoreBackendMigrationDecoratorTest : public testing::Test {
 protected:
  PasswordStoreBackendMigrationDecoratorTest() {
    backend_migration_decorator_ =
        std::make_unique<PasswordStoreBackendMigrationDecorator>(
            CreateBuiltInBackend(), CreateAndroidBackend(), /*prefs=*/nullptr,
            /*is_syncing_passwords_callback=*/base::BindRepeating([]() {
              return false;
            }));
  }

  ~PasswordStoreBackendMigrationDecoratorTest() override {
    backend_migration_decorator()->Shutdown(base::DoNothing());
  }

  PasswordStoreBackend* backend_migration_decorator() {
    return backend_migration_decorator_.get();
  }
  PasswordStoreBackend* built_in_backend() { return built_in_backend_; }
  PasswordStoreBackend* android_backend() { return android_backend_; }

  void AddTestLogins() {
    for (const auto& login : CreateTestLogins()) {
      built_in_backend()->AddLoginAsync(*login, base::DoNothing());
      android_backend()->AddLoginAsync(*login, base::DoNothing());
    }
    RunUntilIdle();
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  std::unique_ptr<PasswordStoreBackend> CreateBuiltInBackend() {
    auto unique_backend = std::make_unique<FakePasswordStoreBackend>();
    built_in_backend_ = unique_backend.get();
    return unique_backend;
  }

  std::unique_ptr<PasswordStoreBackend> CreateAndroidBackend() {
    auto unique_backend = std::make_unique<FakePasswordStoreBackend>();
    android_backend_ = unique_backend.get();
    return unique_backend;
  }

  base::test::SingleThreadTaskEnvironment task_env_;
  raw_ptr<FakePasswordStoreBackend> built_in_backend_;
  raw_ptr<FakePasswordStoreBackend> android_backend_;

  std::unique_ptr<PasswordStoreBackendMigrationDecorator>
      backend_migration_decorator_;
};

TEST_F(PasswordStoreBackendMigrationDecoratorTest,
       UseBuiltInBackendToGetAllLoginsAsync) {
  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins =
      CreateTestLogins();
  AddTestLogins();
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
  backend_migration_decorator()->GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

}  // namespace password_manager
