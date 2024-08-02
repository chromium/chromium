// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_pkcs11_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_pkcs11_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::user_data_auth::TpmTokenInfo;

// On invocation, set |called| to true, and store the result |token_info|
// to the |result|.
void OnTpmTokenInfoGetterCompleted(bool* called,
                                   std::optional<TpmTokenInfo>* result,
                                   std::optional<TpmTokenInfo> token_info) {
  DCHECK(called);
  DCHECK(result);
  *called = true;
  *result = std::move(token_info);
}

// Task runner for handling delayed tasks posted by TPMTokenInfoGetter when
// retrying failed cryptohome method calls. It just records the requested
// delay and immediately runs the task. The task is run asynchronously to be
// closer to what's actually happening in production.
// The delays used by TPMTokenGetter should be monotonically increasing, so
// the fake task runner does not handle task reordering based on the delays.
class FakeTaskRunner : public base::TaskRunner {
 public:
  // |delays|: Vector to which the dalays seen by the task runner are saved.
  explicit FakeTaskRunner(std::vector<int64_t>* delays) : delays_(delays) {}

  FakeTaskRunner(const FakeTaskRunner&) = delete;
  FakeTaskRunner& operator=(const FakeTaskRunner&) = delete;

  // base::TaskRunner overrides:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    delays_->push_back(delay.InMilliseconds());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        from_here, std::move(task));
    return true;
  }

 protected:
  ~FakeTaskRunner() override = default;

 private:
  // The vector of delays.
  raw_ptr<std::vector<int64_t>> delays_;
};

// Implementation of CryptohomePkcs11Client used in these tests.
// TestCryptohomePkcs11Client overrides all CryptohomePkcs11Client methods used
// in TPMTokenInfoGetter tests.
class TestCryptohomePkcs11Client : public FakeCryptohomePkcs11Client {
 public:
  // |account_id|: The user associated with the TPMTokenInfoGetter that will be
  // using the TestCryptohomePkcs11Client. Should be empty for system token.
  explicit TestCryptohomePkcs11Client(const AccountId& account_id)
      : account_id_(account_id),
        get_tpm_token_info_failure_count_(0),
        get_tpm_token_info_not_set_count_(0),
        get_tpm_token_info_succeeded_(false) {}

  TestCryptohomePkcs11Client(const TestCryptohomePkcs11Client&) = delete;
  TestCryptohomePkcs11Client& operator=(const TestCryptohomePkcs11Client&) =
      delete;

  ~TestCryptohomePkcs11Client() override = default;

  void set_get_tpm_token_info_failure_count(int value) {
    ASSERT_GT(value, 0);
    get_tpm_token_info_failure_count_ = value;
  }

  void set_get_tpm_token_info_not_set_count(int value) {
    ASSERT_GT(value, 0);
    get_tpm_token_info_not_set_count_ = value;
  }

  // Sets the tpm tpken info to be reported by the test CryptohomePkcs11Client.
  // If there is |Pkcs11GetTpmTokenInfo| in progress, runs the pending
  // callback with the set tpm token info.
  void SetTpmTokenInfo(const TpmTokenInfo& token_info) {
    tpm_token_info_ = token_info;
    ASSERT_NE(-1, tpm_token_info_->slot());

    InvokeGetTpmTokenInfoCallbackIfReady();
  }

 private:
  // FakeCryptohomePkcs11Client override.
  void Pkcs11GetTpmTokenInfo(
      const ::user_data_auth::Pkcs11GetTpmTokenInfoRequest& request,
      Pkcs11GetTpmTokenInfoCallback callback) override {
    if (request.username().empty()) {
      ASSERT_TRUE(account_id_.empty());
    } else {
      ASSERT_EQ(cryptohome::CreateAccountIdentifierFromAccountId(account_id_)
                    .account_id(),
                request.username());
    }
    HandleGetTpmTokenInfo(std::move(callback));
  }

  // Handles Pkcs11GetTpmTokenInfo calls (both for system and user token). The
  // CryptohomePkcs11Client method overrides should make sure that |account_id_|
  // is properly set before calling this.
  void HandleGetTpmTokenInfo(
      chromeos::DBusMethodCallback<::user_data_auth::Pkcs11GetTpmTokenInfoReply>
          callback) {
    ASSERT_FALSE(get_tpm_token_info_succeeded_);
    ASSERT_TRUE(pending_get_tpm_token_info_callback_.is_null());

    if (get_tpm_token_info_failure_count_ > 0) {
      --get_tpm_token_info_failure_count_;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    if (get_tpm_token_info_not_set_count_ > 0) {
      --get_tpm_token_info_not_set_count_;
      ::user_data_auth::Pkcs11GetTpmTokenInfoReply reply;
      reply.mutable_token_info()->set_slot(-1);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), reply));
      return;
    }

    pending_get_tpm_token_info_callback_ = std::move(callback);
    InvokeGetTpmTokenInfoCallbackIfReady();
  }

  void InvokeGetTpmTokenInfoCallbackIfReady() {
    if (!tpm_token_info_.has_value() || tpm_token_info_->slot() == -1 ||
        pending_get_tpm_token_info_callback_.is_null())
      return;

    get_tpm_token_info_succeeded_ = true;
    // Called synchronously for convenience (to avoid using extra RunLoop in
    // tests). Unlike with other Cryptohome callbacks, TPMTokenInfoGetter does
    // not rely on this callback being called asynchronously.
    ::user_data_auth::Pkcs11GetTpmTokenInfoReply reply;
    reply.mutable_token_info()->CopyFrom(tpm_token_info_.value());
    std::move(pending_get_tpm_token_info_callback_).Run(reply);
  }

  AccountId account_id_;
  int get_tpm_token_info_failure_count_;
  int get_tpm_token_info_not_set_count_;
  bool get_tpm_token_info_succeeded_;
  chromeos::DBusMethodCallback<::user_data_auth::Pkcs11GetTpmTokenInfoReply>
      pending_get_tpm_token_info_callback_;
  std::optional<TpmTokenInfo> tpm_token_info_;
};

class SystemTPMTokenInfoGetterTest : public testing::Test {
 public:
  SystemTPMTokenInfoGetterTest() {
    chromeos::TpmManagerClient::Get()->InitializeFake();
  }

  SystemTPMTokenInfoGetterTest(const SystemTPMTokenInfoGetterTest&) = delete;
  SystemTPMTokenInfoGetterTest& operator=(const SystemTPMTokenInfoGetterTest&) =
      delete;

  ~SystemTPMTokenInfoGetterTest() override {
    chromeos::TpmManagerClient::Get()->Shutdown();
  }

  void SetUp() override {
    cryptohome_client_ =
        std::make_unique<TestCryptohomePkcs11Client>(EmptyAccountId());
    tpm_token_info_getter_ = TPMTokenInfoGetter::CreateForSystemToken(
        cryptohome_client_.get(),
        scoped_refptr<base::TaskRunner>(new FakeTaskRunner(&delays_)));
  }

 protected:
  std::unique_ptr<TestCryptohomePkcs11Client> cryptohome_client_;
  std::unique_ptr<TPMTokenInfoGetter> tpm_token_info_getter_;
  std::vector<int64_t> delays_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class UserTPMTokenInfoGetterTest : public testing::Test {
 public:
  UserTPMTokenInfoGetterTest()
      : account_id_(AccountId::FromUserEmail("user@gmail.com")) {
    chromeos::TpmManagerClient::Get()->InitializeFake();
  }

  UserTPMTokenInfoGetterTest(const UserTPMTokenInfoGetterTest&) = delete;
  UserTPMTokenInfoGetterTest& operator=(const UserTPMTokenInfoGetterTest&) =
      delete;

  ~UserTPMTokenInfoGetterTest() override {
    chromeos::TpmManagerClient::Get()->Shutdown();
  }

  void SetUp() override {
    cryptohome_client_ =
        std::make_unique<TestCryptohomePkcs11Client>(account_id_);
    tpm_token_info_getter_ = TPMTokenInfoGetter::CreateForUserToken(
        account_id_, cryptohome_client_.get(),
        scoped_refptr<base::TaskRunner>(new FakeTaskRunner(&delays_)));
    tpm_token_info_getter_->set_nss_slots_software_fallback_for_testing(false);
  }

 protected:
  std::unique_ptr<TestCryptohomePkcs11Client> cryptohome_client_;
  std::unique_ptr<TPMTokenInfoGetter> tpm_token_info_getter_;

  const AccountId account_id_;
  std::vector<int64_t> delays_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SystemTPMTokenInfoGetterTest, BasicFlow) {
  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TokenSlotIdEqualsZero) {
  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_0");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(0);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_0", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(0, result->slot());

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TPMNotEnabled) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(false);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(completed);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TPMNotOwnedSystemSlotFallbackEnabled) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(false);
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(false);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->set_nss_slots_software_fallback_for_testing(true);
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TPMOwnedSystemSlotFallbackEnabled) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(true);
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(true);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->set_nss_slots_software_fallback_for_testing(true);
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TpmEnabledCallFails) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->set_non_nonsensitive_status_dbus_error_count(1);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + std::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyNotReady) {
  cryptohome_client_->set_get_tpm_token_info_not_set_count(1);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + std::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyFails) {
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + std::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, RetryDelaysIncreaseExponentially) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->set_non_nonsensitive_status_dbus_error_count(2);
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);
  cryptohome_client_->set_get_tpm_token_info_not_set_count(3);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  int64_t kExpectedDelays[] = {100, 200, 400, 800, 1600, 3200};
  ASSERT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + std::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, RetryDelayBounded) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->set_non_nonsensitive_status_dbus_error_count(4);
  cryptohome_client_->set_get_tpm_token_info_failure_count(5);
  cryptohome_client_->set_get_tpm_token_info_not_set_count(6);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);

  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  int64_t kExpectedDelays[] = {100,    200,    400,    800,    1600,
                               3200,   6400,   12800,  25600,  51200,
                               102400, 204800, 300000, 300000, 300000};
  ASSERT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + std::size(kExpectedDelays)),
            delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, BasicFlow) {
  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);

  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyFails) {
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);

  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + std::size(kExpectedDelays)),
            delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyNotReady) {
  cryptohome_client_->set_get_tpm_token_info_not_set_count(1);

  bool completed = false;
  std::optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  TpmTokenInfo fake_token_info;
  fake_token_info.set_label("TOKEN_1");
  fake_token_info.set_user_pin("2222");
  fake_token_info.set_slot(1);

  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label());
  EXPECT_EQ("2222", result->user_pin());
  EXPECT_EQ(1, result->slot());

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + std::size(kExpectedDelays)),
            delays_);
}

}  // namespace
}  // namespace ash
