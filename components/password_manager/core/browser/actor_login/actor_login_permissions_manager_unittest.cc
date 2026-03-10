// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager_impl.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {
namespace {

using ::password_manager::IsAccountStore;
using ::password_manager::PasswordForm;
using ::password_manager::TestPasswordStore;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pointee;
using testing::Return;

class MockObserver : public ActorLoginPermissionsManager::Observer {
 public:
  MOCK_METHOD(void, OnPermissionsChanged, (), (override));
};

PasswordForm CreateApprovedForm(const std::string& signon_realm,
                                const std::u16string& username) {
  PasswordForm form =
      CreateSavedPasswordForm(GURL(signon_realm), username, u"password");
  form.actor_login_approved = true;
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

class ActorLoginPermissionsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    test_sync_service_.SetSignedIn(signin::ConsentLevel::kSync);
    profile_store_->Init(/*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*affiliated_match_helper=*/nullptr);
    permissions_manager_ = std::make_unique<ActorLoginPermissionsManagerImpl>(
        &affiliation_service_, profile_store_, account_store_);
    WaitForPasswordStore();
  }

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    WaitForPasswordStore();
  }

  syncer::TestSyncService* GetSyncService() { return &test_sync_service_; }

  void WaitForPasswordStore() {
    // `PasswordStore` doesn't have a good signal to wait for, hence
    // `RunUntilIdle`.
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  affiliations::FakeAffiliationService affiliation_service_;
  syncer::TestSyncService test_sync_service_;
  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
  std::unique_ptr<ActorLoginPermissionsManagerImpl> permissions_manager_;
};

TEST_F(ActorLoginPermissionsManagerTest, InitiallyEmpty) {
  EXPECT_TRUE(
      permissions_manager_->GetAllPermissions(GetSyncService()).empty());
}

TEST_F(ActorLoginPermissionsManagerTest, GetAll) {
  base::RunLoop run_loop;
  MockObserver observer;
  permissions_manager_->AddObserver(&observer);

  // The observer is notified for each store change. Quit after the second one.
  EXPECT_CALL(observer, OnPermissionsChanged)
      .WillOnce(Return())
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));

  profile_store_->AddLogin(CreateApprovedForm("https://example.com", u"user1"));
  account_store_->AddLogin(CreateApprovedForm("https://example.com", u"user2"));

  run_loop.Run();

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(permissions_manager_->GetAllPermissions(GetSyncService()).size(),
            2u);
#else
  // Permissions rely on passwords grouper to get credentials and the grouper is
  // not available on Android. We still want to be able to build on Android but
  // the actual support needs to be implemented.
  EXPECT_THAT(permissions_manager_->GetAllPermissions(GetSyncService()),
              IsEmpty());
#endif
}

TEST_F(ActorLoginPermissionsManagerTest, RevokePermission) {
  base::RunLoop add_run_loop;
  MockObserver observer;
  permissions_manager_->AddObserver(&observer);

  // Wait until the first permission is added.
  EXPECT_CALL(observer, OnPermissionsChanged)
      .WillOnce(testing::Invoke(&add_run_loop, &base::RunLoop::Quit));
  profile_store_->AddLogin(CreateApprovedForm("https://example.com", u"user1"));
  add_run_loop.Run();

#if !BUILDFLAG(IS_ANDROID)
  ASSERT_EQ(permissions_manager_->GetAllPermissions(GetSyncService()).size(),
            1u);

  // Wait until the permission is revoked.
  base::RunLoop revoke_run_loop;
  EXPECT_CALL(observer, OnPermissionsChanged)
      .WillOnce(testing::Invoke(&revoke_run_loop, &base::RunLoop::Quit));
  permissions_manager_->RevokePermission("https://example.com/");
  revoke_run_loop.Run();

  EXPECT_THAT(permissions_manager_->GetAllPermissions(GetSyncService()),
              IsEmpty());
#else
  // Permissions rely on passwords grouper to get credentials and the grouper is
  // not available on Android. We still want to be able to build on Android but
  // the actual support needs to be implemented.
  EXPECT_THAT(permissions_manager_->GetAllPermissions(GetSyncService()),
              IsEmpty());
#endif
}

}  // namespace actor_login
