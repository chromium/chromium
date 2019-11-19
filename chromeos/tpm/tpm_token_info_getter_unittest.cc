// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/tpm/tpm_token_info_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using TpmTokenInfo = chromeos::CryptohomeClient::TpmTokenInfo;

// On invocation, set |called| to true, and store the result |token_info|
// to the |result|.
void OnTpmTokenInfoGetterCompleted(bool* called,
                                   base::Optional<TpmTokenInfo>* result,
                                   base::Optional<TpmTokenInfo> token_info) {
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

  // base::TaskRunner overrides:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    delays_->push_back(delay.InMilliseconds());
    base::ThreadTaskRunnerHandle::Get()->PostTask(from_here, std::move(task));
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }

 protected:
  ~FakeTaskRunner() override = default;

 private:
  // The vector of delays.
  std::vector<int64_t>* delays_;

  DISALLOW_COPY_AND_ASSIGN(FakeTaskRunner);
};

// Implementation of CryptohomeClient used in these tests. Note that
// TestCryptohomeClient implements FakeCryptohomeClient purely for convenience
// of not having to implement whole CryptohomeClient interface.
// TestCryptohomeClient overrides all CryptohomeClient methods used in
// TPMTokenInfoGetter tests.
class TestCryptohomeClient : public chromeos::FakeCryptohomeClient {
 public:
  // |account_id|: The user associated with the TPMTokenInfoGetter that will be
  // using the TestCryptohomeClient. Should be empty for system token.
  explicit TestCryptohomeClient(const AccountId& account_id)
      : account_id_(account_id),
        tpm_is_enabled_(true),
        tpm_is_enabled_failure_count_(0),
        tpm_is_enabled_succeeded_(false),
        get_tpm_token_info_failure_count_(0),
        get_tpm_token_info_not_set_count_(0),
        get_tpm_token_info_succeeded_(false) {}

  ~TestCryptohomeClient() override = default;

  void set_tpm_is_enabled(bool value) {
    tpm_is_enabled_ = value;
  }

  void set_tpm_is_enabled_failure_count(int value) {
    ASSERT_GT(value, 0);
    tpm_is_enabled_failure_count_ = value;
  }

  void set_get_tpm_token_info_failure_count(int value) {
    ASSERT_GT(value, 0);
    get_tpm_token_info_failure_count_ = value;
  }

  void set_get_tpm_token_info_not_set_count(int value) {
    ASSERT_GT(value, 0);
    get_tpm_token_info_not_set_count_ = value;
  }

  // Sets the tpm tpken info to be reported by the test CryptohomeClient.
  // If there is |Pkcs11GetTpmTokenInfo| in progress, runs the pending
  // callback with the set tpm token info.
  void SetTpmTokenInfo(const TpmTokenInfo& token_info) {
    tpm_token_info_ = token_info;
    ASSERT_NE(-1, tpm_token_info_->slot);

    InvokeGetTpmTokenInfoCallbackIfReady();
  }

 private:
  // FakeCryptohomeClient override.
  void TpmIsEnabled(chromeos::DBusMethodCallback<bool> callback) override {
    ASSERT_FALSE(tpm_is_enabled_succeeded_);
    base::Optional<bool> result;
    if (tpm_is_enabled_failure_count_ > 0) {
      --tpm_is_enabled_failure_count_;
    } else {
      tpm_is_enabled_succeeded_ = true;
      result.emplace(tpm_is_enabled_);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  void Pkcs11GetTpmTokenInfo(
      chromeos::DBusMethodCallback<TpmTokenInfo> callback) override {
    ASSERT_TRUE(account_id_.empty());

    HandleGetTpmTokenInfo(std::move(callback));
  }

  void Pkcs11GetTpmTokenInfoForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      chromeos::DBusMethodCallback<TpmTokenInfo> callback) override {
    ASSERT_FALSE(cryptohome_id.account_id().empty());
    ASSERT_EQ(cryptohome::CreateAccountIdentifierFromAccountId(account_id_)
                  .account_id(),
              cryptohome_id.account_id());

    HandleGetTpmTokenInfo(std::move(callback));
  }

  // Handles Pkcs11GetTpmTokenInfo calls (both for system and user token). The
  // CryptohomeClient method overrides should make sure that |account_id_| is
  // properly set before calling this.
  void HandleGetTpmTokenInfo(
      chromeos::DBusMethodCallback<TpmTokenInfo> callback) {
    ASSERT_TRUE(tpm_is_enabled_succeeded_);
    ASSERT_FALSE(get_tpm_token_info_succeeded_);
    ASSERT_TRUE(pending_get_tpm_token_info_callback_.is_null());

    if (get_tpm_token_info_failure_count_ > 0) {
      --get_tpm_token_info_failure_count_;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    if (get_tpm_token_info_not_set_count_ > 0) {
      --get_tpm_token_info_not_set_count_;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    TpmTokenInfo{std::string() /* label */,
                                                 std::string() /* user_pin */,
                                                 -1 /* slot */}));
      return;
    }

    pending_get_tpm_token_info_callback_ = std::move(callback);
    InvokeGetTpmTokenInfoCallbackIfReady();
  }

  void InvokeGetTpmTokenInfoCallbackIfReady() {
    if (!tpm_token_info_.has_value() || tpm_token_info_->slot == -1 ||
        pending_get_tpm_token_info_callback_.is_null())
      return;

    get_tpm_token_info_succeeded_ = true;
    // Called synchronously for convenience (to avoid using extra RunLoop in
    // tests). Unlike with other Cryptohome callbacks, TPMTokenInfoGetter does
    // not rely on this callback being called asynchronously.
    std::move(pending_get_tpm_token_info_callback_).Run(tpm_token_info_);
  }

  AccountId account_id_;
  bool tpm_is_enabled_;
  int tpm_is_enabled_failure_count_;
  bool tpm_is_enabled_succeeded_;
  int get_tpm_token_info_failure_count_;
  int get_tpm_token_info_not_set_count_;
  bool get_tpm_token_info_succeeded_;
  chromeos::DBusMethodCallback<TpmTokenInfo>
      pending_get_tpm_token_info_callback_;
  base::Optional<TpmTokenInfo> tpm_token_info_;

  DISALLOW_COPY_AND_ASSIGN(TestCryptohomeClient);
};

class SystemTPMTokenInfoGetterTest : public testing::Test {
 public:
  SystemTPMTokenInfoGetterTest() = default;
  ~SystemTPMTokenInfoGetterTest() override = default;

  void SetUp() override {
    cryptohome_client_.reset(new TestCryptohomeClient(EmptyAccountId()));
    tpm_token_info_getter_ =
        chromeos::TPMTokenInfoGetter::CreateForSystemToken(
            cryptohome_client_.get(),
            scoped_refptr<base::TaskRunner>(new FakeTaskRunner(&delays_)));
  }

 protected:
  std::unique_ptr<TestCryptohomeClient> cryptohome_client_;
  std::unique_ptr<chromeos::TPMTokenInfoGetter> tpm_token_info_getter_;
  std::vector<int64_t> delays_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(SystemTPMTokenInfoGetterTest);
};

class UserTPMTokenInfoGetterTest : public testing::Test {
 public:
  UserTPMTokenInfoGetterTest()
      : account_id_(AccountId::FromUserEmail("user@gmail.com")) {}
  ~UserTPMTokenInfoGetterTest() override = default;

  void SetUp() override {
    cryptohome_client_.reset(new TestCryptohomeClient(account_id_));
    tpm_token_info_getter_ = chromeos::TPMTokenInfoGetter::CreateForUserToken(
        account_id_, cryptohome_client_.get(),
        scoped_refptr<base::TaskRunner>(new FakeTaskRunner(&delays_)));
  }

 protected:
  std::unique_ptr<TestCryptohomeClient> cryptohome_client_;
  std::unique_ptr<chromeos::TPMTokenInfoGetter> tpm_token_info_getter_;

  const AccountId account_id_;
  std::vector<int64_t> delays_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(UserTPMTokenInfoGetterTest);
};

TEST_F(SystemTPMTokenInfoGetterTest, BasicFlow) {
  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TokenSlotIdEqualsZero) {
  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_0", "2222", 0};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_0", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(0, result->slot);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TPMNotEnabled) {
  cryptohome_client_->set_tpm_is_enabled(false);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(completed);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, TpmEnabledCallFails) {
  cryptohome_client_->set_tpm_is_enabled_failure_count(1);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + base::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyNotReady) {
  cryptohome_client_->set_get_tpm_token_info_not_set_count(1);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + base::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyFails) {
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + base::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, RetryDelaysIncreaseExponentially) {
  cryptohome_client_->set_tpm_is_enabled_failure_count(2);
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);
  cryptohome_client_->set_get_tpm_token_info_not_set_count(3);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  int64_t kExpectedDelays[] = {100, 200, 400, 800, 1600, 3200};
  ASSERT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + base::size(kExpectedDelays)),
            delays_);
}

TEST_F(SystemTPMTokenInfoGetterTest, RetryDelayBounded) {
  cryptohome_client_->set_tpm_is_enabled_failure_count(4);
  cryptohome_client_->set_get_tpm_token_info_failure_count(5);
  cryptohome_client_->set_get_tpm_token_info_not_set_count(6);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  int64_t kExpectedDelays[] = {100,    200,    400,    800,    1600,
                               3200,   6400,   12800,  25600,  51200,
                               102400, 204800, 300000, 300000, 300000};
  ASSERT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + base::size(kExpectedDelays)),
            delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, BasicFlow) {
  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  EXPECT_EQ(std::vector<int64_t>(), delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyFails) {
  cryptohome_client_->set_get_tpm_token_info_failure_count(1);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + base::size(kExpectedDelays)),
            delays_);
}

TEST_F(UserTPMTokenInfoGetterTest, GetTpmTokenInfoInitiallyNotReady) {
  cryptohome_client_->set_get_tpm_token_info_not_set_count(1);

  bool completed = false;
  base::Optional<TpmTokenInfo> result;
  tpm_token_info_getter_->Start(
      base::BindOnce(&OnTpmTokenInfoGetterCompleted, &completed, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed);

  const TpmTokenInfo fake_token_info = {"TOKEN_1", "2222", 1};
  cryptohome_client_->SetTpmTokenInfo(fake_token_info);

  EXPECT_TRUE(completed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("TOKEN_1", result->label);
  EXPECT_EQ("2222", result->user_pin);
  EXPECT_EQ(1, result->slot);

  const int64_t kExpectedDelays[] = {100};
  EXPECT_EQ(std::vector<int64_t>(kExpectedDelays,
                                 kExpectedDelays + base::size(kExpectedDelays)),
            delays_);
}

}  // namespace
