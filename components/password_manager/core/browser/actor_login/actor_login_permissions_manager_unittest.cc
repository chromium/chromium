// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permissions_manager_impl.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"
#include "components/password_manager/core/browser/actor_login/test/actor_login_test_util.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_permission_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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
    ON_CALL(actor_login_permission_service_, ListAllPermissions)
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<0>(
            std::vector<FederatedPermission>()));
    permissions_manager_ = std::make_unique<ActorLoginPermissionsManagerImpl>(
        &affiliation_service_, &actor_login_permission_service_, profile_store_,
        account_store_);
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
  testing::NiceMock<MockActorLoginPermissionService>
      actor_login_permission_service_;
  std::unique_ptr<ActorLoginPermissionsManagerImpl> permissions_manager_;
};

TEST_F(ActorLoginPermissionsManagerTest, InitiallyEmpty) {
  base::test::TestFuture<base::flat_set<password_manager::ActorLoginPermission>>
      future;
  permissions_manager_->GetAllPermissions(GetSyncService(),
                                          future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ActorLoginPermissionsManagerTest, GetAllPermissions_OnlyPassword) {
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

  base::test::TestFuture<base::flat_set<password_manager::ActorLoginPermission>>
      future;
  permissions_manager_->GetAllPermissions(GetSyncService(),
                                          future.GetCallback());
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(future.Get().size(), 2u);
#else
  // Permissions rely on passwords grouper to get credentials and the grouper is
  // not available on Android. We still want to be able to build on Android but
  // the actual support needs to be implemented.
  EXPECT_THAT(future.Get(), IsEmpty());
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
  base::test::TestFuture<base::flat_set<password_manager::ActorLoginPermission>>
      add_future;
  permissions_manager_->GetAllPermissions(GetSyncService(),
                                          add_future.GetCallback());
  ASSERT_EQ(add_future.Get().size(), 1u);

  // Wait until the permission is revoked.
  base::RunLoop revoke_run_loop;
  EXPECT_CALL(observer, OnPermissionsChanged)
      .WillOnce(testing::Invoke(&revoke_run_loop, &base::RunLoop::Quit));

  EXPECT_CALL(
      actor_login_permission_service_,
      DeletePermission(url::Origin::Create(GURL("https://example.com/")), _));

  permissions_manager_->RevokePermission("https://example.com/");
  revoke_run_loop.Run();

  base::test::TestFuture<base::flat_set<password_manager::ActorLoginPermission>>
      revoke_future;
  permissions_manager_->GetAllPermissions(GetSyncService(),
                                          revoke_future.GetCallback());
  EXPECT_THAT(revoke_future.Get(), IsEmpty());
#else
  // Permissions rely on passwords grouper to get credentials and the grouper is
  // not available on Android. We still want to be able to build on Android but
  // the actual support needs to be implemented.
  base::test::TestFuture<base::flat_set<password_manager::ActorLoginPermission>>
      future;
  permissions_manager_->GetAllPermissions(GetSyncService(),
                                          future.GetCallback());
  EXPECT_THAT(future.Get(), IsEmpty());
#endif
}

TEST_F(ActorLoginPermissionsManagerTest, GetAllPermissions_OnlyFederated) {
  FederatedPermission federated_permission_1;
  federated_permission_1.rp_embedder_origin =
      url::Origin::Create(GURL("https://example.com/"));
  federated_permission_1.chosen_account_email = "user1";

  FederatedPermission federated_permission_2;
  federated_permission_2.rp_embedder_origin =
      url::Origin::Create(GURL("https://example.com/"));
  federated_permission_2.chosen_account_email = "user2";

  EXPECT_CALL(actor_login_permission_service_, ListAllPermissions)
      .WillOnce(base::test::RunOnceCallback<0>(std::vector<FederatedPermission>{
          federated_permission_1, federated_permission_2}));

  base::test::TestFuture<base::flat_set<password_manager::ActorLoginPermission>>
      future;
  permissions_manager_->GetAllPermissions(GetSyncService(),
                                          future.GetCallback());
  base::flat_set<password_manager::ActorLoginPermission> permissions =
      future.Get();

  EXPECT_EQ(permissions.size(), 2u);

  EXPECT_THAT(base::ToVector(permissions,
                             [](const auto& p) {
                               return std::pair(p.username,
                                                p.domain_info.signon_realm);
                             }),
              testing::UnorderedElementsAre(
                  std::pair(u"user1", "https://example.com/"),
                  std::pair(u"user2", "https://example.com/")));
}

TEST_F(ActorLoginPermissionsManagerTest,
       GetAllPermissions_FederatedAndPassword) {
  base::RunLoop run_loop;
  MockObserver observer;
  permissions_manager_->AddObserver(&observer);

  EXPECT_CALL(observer, OnPermissionsChanged)
      .WillOnce(Return())
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));

  profile_store_->AddLogin(CreateApprovedForm("https://example.com", u"user1"));
  profile_store_->AddLogin(
      CreateApprovedForm("https://password.com", u"password_user"));

  run_loop.Run();

  FederatedPermission federated_permission_1;
  federated_permission_1.rp_embedder_origin =
      url::Origin::Create(GURL("https://example.com/"));
  federated_permission_1.chosen_account_email = "user1";

  FederatedPermission federated_permission_2;
  federated_permission_2.rp_embedder_origin =
      url::Origin::Create(GURL("https://example.com/"));
  federated_permission_2.chosen_account_email = "user2";

  FederatedPermission federated_permission_3;
  federated_permission_3.rp_embedder_origin =
      url::Origin::Create(GURL("https://other.com/"));
  federated_permission_3.chosen_account_email = "user1";

  EXPECT_CALL(actor_login_permission_service_, ListAllPermissions)
      .WillOnce(base::test::RunOnceCallback<0>(std::vector<FederatedPermission>{
          federated_permission_1, federated_permission_2,
          federated_permission_3}));

  base::test::TestFuture<base::flat_set<password_manager::ActorLoginPermission>>
      future;
  permissions_manager_->GetAllPermissions(GetSyncService(),
                                          future.GetCallback());
  base::flat_set<password_manager::ActorLoginPermission> permissions =
      future.Get();

#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(permissions.size(), 4u);
  EXPECT_THAT(base::ToVector(permissions,
                             [](const auto& p) {
                               return std::pair(p.username,
                                                p.domain_info.signon_realm);
                             }),
              testing::UnorderedElementsAre(
                  std::pair(u"user1", "https://example.com/"),
                  std::pair(u"password_user", "https://password.com/"),
                  std::pair(u"user2", "https://example.com/"),
                  std::pair(u"user1", "https://other.com/")));
#else
  // Grouper is not supported on Android yet, so password permissions are not
  // returned.
  EXPECT_EQ(permissions.size(), 3u);
  EXPECT_THAT(
      base::ToVector(permissions,
                     [](const auto& p) {
                       return std::pair(p.username, p.domain_info.signon_realm);
                     }),
      testing::UnorderedElementsAre(std::pair(u"user1", "https://example.com/"),
                                    std::pair(u"user2", "https://example.com/"),
                                    std::pair(u"user1", "https://other.com/")));
#endif
}

TEST_F(ActorLoginPermissionsManagerTest,
       RevokePermission_FederatedPermission_NotifiesObserverOnSuccess) {
  MockObserver observer;
  permissions_manager_->AddObserver(&observer);

  EXPECT_CALL(
      actor_login_permission_service_,
      DeletePermission(url::Origin::Create(GURL("https://example.com/")), _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  EXPECT_CALL(observer, OnPermissionsChanged);

  permissions_manager_->RevokePermission("https://example.com/");
}

TEST_F(ActorLoginPermissionsManagerTest,
       RevokePermission_FederatedPermission_DoesNotNotifyObserverOnFailure) {
  MockObserver observer;
  permissions_manager_->AddObserver(&observer);

  EXPECT_CALL(
      actor_login_permission_service_,
      DeletePermission(url::Origin::Create(GURL("https://example.com/")), _))
      .WillOnce(base::test::RunOnceCallback<1>(false));

  EXPECT_CALL(observer, OnPermissionsChanged).Times(0);

  permissions_manager_->RevokePermission("https://example.com/");
}

}  // namespace actor_login
