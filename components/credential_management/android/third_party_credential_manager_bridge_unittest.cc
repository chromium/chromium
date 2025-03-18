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
#include "base/types/pass_key.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  void CreateBridge(ThirdPartyCredentialManagerBridge* bridge) override {}

  void Get(const std::string& origin) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();

    if (simulate_errors_) {
      bridge_->OnGetPasswordCredentialError(env);
      return;
    }
    jstring username_java = env->NewStringUTF(kTestUsername.c_str());
    jstring password_java = env->NewStringUTF(kTestPassword.c_str());
    jstring origin_java = env->NewStringUTF(kTestOrigin.c_str());
    base::android::JavaParamRef<jstring> username_param_ref(env, username_java);
    base::android::JavaParamRef<jstring> password_param_ref(env, password_java);
    base::android::JavaParamRef<jstring> origin_param_ref(env, origin_java);
    bridge_->OnPasswordCredentialReceived(env, username_param_ref,
                                          password_param_ref, origin_param_ref);
  }

  void Store(const std::string& username,
             const std::string& password,
             const std::string& origin) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    bridge_->OnCreateCredentialResponse(env, !simulate_errors_);
  }

  void set_bridge(ThirdPartyCredentialManagerBridge* bridge) {
    bridge_ = bridge;
  }

  void set_error_simulation(bool simulate_errors) {
    simulate_errors_ = simulate_errors;
  }

  static constexpr std::string kTestUsername = "username";
  static constexpr std::string kTestPassword = "password";
  static constexpr std::string kTestOrigin = "origin.com";

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

TEST_F(ThirdPartyCredentialManagerBridgeTest, testSuccessfulGetCall) {
  base::RunLoop run_loop;
  base::MockCallback<GetCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(false);

  bridge()->Create(mock_callback.Get());

  EXPECT_CALL(
      mock_callback,
      Run(password_manager::CredentialManagerError::SUCCESS, testing::_))
      .WillOnce(testing::Invoke([&]() { run_loop.Quit(); }));
  bridge()->Get(FakeJniDelegate::kTestOrigin);
  run_loop.Run();
}

TEST_F(ThirdPartyCredentialManagerBridgeTest, testUnuccessfulGetCall) {
  base::RunLoop run_loop;
  base::MockCallback<GetCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(true);

  bridge()->Create(mock_callback.Get());

  EXPECT_CALL(
      mock_callback,
      Run(password_manager::CredentialManagerError::UNKNOWN, testing::_))
      .WillOnce(testing::Invoke([&]() { run_loop.Quit(); }));
  bridge()->Get(FakeJniDelegate::kTestOrigin);
  run_loop.Run();
}

TEST_F(ThirdPartyCredentialManagerBridgeTest, testSuccessfulStoreCall) {
  base::RunLoop run_loop;
  base::MockCallback<StoreCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(false);

  bridge()->Create(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run()).WillOnce(testing::Invoke([&]() {
    run_loop.Quit();
  }));
  bridge()->Store(FakeJniDelegate::kTestUsername,
                  FakeJniDelegate::kTestPassword, FakeJniDelegate::kTestOrigin);
  run_loop.Run();
}

TEST_F(ThirdPartyCredentialManagerBridgeTest, testUnuccessfulStoreCall) {
  base::RunLoop run_loop;
  base::MockCallback<StoreCallback> mock_callback;
  fake_jni_delegate().set_error_simulation(true);

  bridge()->Create(mock_callback.Get());

  EXPECT_CALL(mock_callback, Run()).WillOnce(testing::Invoke([&]() {
    run_loop.Quit();
  }));
  bridge()->Store(FakeJniDelegate::kTestUsername,
                  FakeJniDelegate::kTestPassword, FakeJniDelegate::kTestOrigin);
  run_loop.Run();
}

}  // namespace credential_management
