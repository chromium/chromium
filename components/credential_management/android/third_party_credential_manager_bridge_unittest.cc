// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/third_party_credential_manager_bridge.h"

#include <jni.h>

#include <algorithm>
#include <memory>

#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "components/credential_management/android/password_credential_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const std::u16string kTestUsername = u"username";
const std::u16string kTestPassword = u"password";
const std::string kTestOrigin = "https://origin.com";
}  // namespace
namespace credential_management {
using StoreCallback = base::OnceCallback<void()>;
using GetCallback = base::OnceCallback<void(
    password_manager::CredentialManagerError,
    const std::optional<password_manager::CredentialInfo>&)>;

using JniDelegate = ThirdPartyCredentialManagerBridge::JniDelegate;

class FakeJniDelegate : public JniDelegate {
 public:
  FakeJniDelegate() = default;
  FakeJniDelegate(const FakeJniDelegate&) = delete;
  FakeJniDelegate& operator=(const FakeJniDelegate&) = delete;
  ~FakeJniDelegate() override = default;

  void Get(bool is_auto_select_allowed,
           bool include_passwords,
           const std::vector<GURL>& federations,
           const std::string& origin,
           base::OnceCallback<void(PasswordCredentialResponse)>
               completion_callback) override {
    if (simulate_errors_) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(completion_callback),
                         PasswordCredentialResponse(false, u"", u"")));
      return;
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback),
                                  PasswordCredentialResponse(
                                      true, kTestUsername, kTestPassword)));
  }

  void Store(const std::u16string& username,
             const std::u16string& password,
             const std::string& origin,
             base::OnceCallback<void(bool)> completion_callback) override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback), simulate_errors_));
  }

  void set_bridge(ThirdPartyCredentialManagerBridge* bridge) {
    bridge_ = bridge;
  }

  void set_error_simulation(bool simulate_errors) {
    simulate_errors_ = simulate_errors;
  }

 private:
  // The owning native ThirdPartyCredentialManagerBridge.
  raw_ptr<ThirdPartyCredentialManagerBridge> bridge_;
  bool simulate_errors_;
};

class ThirdPartyCredentialManagerBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    auto jni_delegate = std::make_unique<FakeJniDelegate>();
    fake_jni_delegate_ = jni_delegate.get();
    bridge_ = std::make_unique<ThirdPartyCredentialManagerBridge>(
        base::PassKey<class ThirdPartyCredentialManagerBridgeTest>(),
        std::move(jni_delegate));
    fake_jni_delegate_->set_bridge(bridge());
  }

  FakeJniDelegate& fake_jni_delegate() { return *fake_jni_delegate_; }

  ThirdPartyCredentialManagerBridge* bridge() { return bridge_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<FakeJniDelegate> fake_jni_delegate_;
  std::unique_ptr<ThirdPartyCredentialManagerBridge> bridge_;
};

TEST_F(ThirdPartyCredentialManagerBridgeTest, TestSuccessfulGetCall) {
  base::RunLoop run_loop;
  base::MockCallback<GetCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(false);

  EXPECT_CALL(
      mock_callback,
      Run(password_manager::CredentialManagerError::SUCCESS, testing::_))
      .WillOnce([&]() { run_loop.Quit(); });
  bridge()->Get(/*is_auto_select_allowed=*/false, /*include_passwords=*/true,
                /*federations=*/{}, kTestOrigin, mock_callback.Get());
  run_loop.Run();
}

TEST_F(ThirdPartyCredentialManagerBridgeTest, TestUnuccessfulGetCall) {
  base::RunLoop run_loop;
  base::MockCallback<GetCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(true);

  EXPECT_CALL(
      mock_callback,
      Run(password_manager::CredentialManagerError::UNKNOWN, testing::_))
      .WillOnce([&]() { run_loop.Quit(); });
  bridge()->Get(/*is_auto_select_allowed=*/true, /*include_passwords=*/true,
                /*federations=*/{}, kTestOrigin, mock_callback.Get());
  run_loop.Run();
}

TEST_F(ThirdPartyCredentialManagerBridgeTest,
       TestGetCallWithoutPasswordsFails) {
  base::test::TestFuture<password_manager::CredentialManagerError,
                         const std::optional<password_manager::CredentialInfo>&>
      future;

  bridge()->Get(/*is_auto_select_allowed=*/true, /*include_passwords=*/false,
                /*federations=*/{}, kTestOrigin, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<0>(), password_manager::CredentialManagerError::UNKNOWN);
  EXPECT_FALSE(future.Get<1>().has_value());
}

TEST_F(ThirdPartyCredentialManagerBridgeTest, TestSuccessfulStoreCall) {
  base::RunLoop run_loop;
  base::MockCallback<StoreCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(false);

  EXPECT_CALL(mock_callback, Run()).WillOnce([&]() { run_loop.Quit(); });
  bridge()->Store(kTestUsername, kTestPassword, kTestOrigin,
                  mock_callback.Get());
  run_loop.Run();
}

TEST_F(ThirdPartyCredentialManagerBridgeTest, TestUnuccessfulStoreCall) {
  base::RunLoop run_loop;
  base::MockCallback<StoreCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(true);

  EXPECT_CALL(mock_callback, Run()).WillOnce([&]() { run_loop.Quit(); });
  bridge()->Store(kTestUsername, kTestPassword, kTestOrigin,
                  mock_callback.Get());
  run_loop.Run();
}

TEST_F(ThirdPartyCredentialManagerBridgeTest, TestMultipleCalls) {
  base::RunLoop run_loop_store;
  base::RunLoop run_loop_get;
  base::MockCallback<StoreCallback> mock_store_callback;
  base::MockCallback<GetCallback> mock_get_callback;
  fake_jni_delegate().set_error_simulation(false);

  EXPECT_CALL(mock_store_callback, Run()).WillOnce([&]() {
    run_loop_store.Quit();
  });
  bridge()->Store(kTestUsername, kTestPassword, kTestOrigin,
                  mock_store_callback.Get());
  run_loop_store.Run();

  EXPECT_CALL(
      mock_get_callback,
      Run(password_manager::CredentialManagerError::SUCCESS, testing::_))
      .WillOnce([&]() { run_loop_get.Quit(); });

  bridge()->Get(/*is_auto_select_allowed=*/true, /*include_passwords=*/true,
                /*federations=*/{}, kTestOrigin, mock_get_callback.Get());
  run_loop_get.Run();
}

}  // namespace credential_management
