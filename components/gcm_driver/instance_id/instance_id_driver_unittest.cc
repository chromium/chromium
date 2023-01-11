// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_driver.h"

#include <stddef.h>

#include <cmath>
#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_buildflags.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
#include "components/gcm_driver/instance_id/instance_id_android.h"
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#endif  // BUILDFLAG(USE_GCM_FROM_PLATFORM)

namespace instance_id {

namespace {

const char kTestAppID1[] = "TestApp1";
const char kTestAppID2[] = "TestApp2";
const char kAuthorizedEntity1[] = "Sender 1";
const char kAuthorizedEntity2[] = "Sender 2";
const char kScope1[] = "GCM1";
const char kScope2[] = "FooBar";

bool VerifyInstanceID(const std::string& str) {
  // Checks the length.
  if (str.length() != static_cast<size_t>(
          std::ceil(InstanceID::kInstanceIDByteLength * 8 / 6.0)))
    return false;

  // Checks if it is URL-safe base64 encoded.
  for (auto ch : str) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) &&
        ch != '_' && ch != '-')
      return false;
  }
  return true;
}

}  // namespace

class InstanceIDDriverTest : public testing::Test {
 public:
  InstanceIDDriverTest();

  InstanceIDDriverTest(const InstanceIDDriverTest&) = delete;
  InstanceIDDriverTest& operator=(const InstanceIDDriverTest&) = delete;

  ~InstanceIDDriverTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void WaitForAsyncOperation();

  // Recreates InstanceIDDriver to simulate restart.
  void RecreateInstanceIDDriver();

  // Sync wrappers for async version.
  std::string GetID(InstanceID* instance_id);
  base::Time GetCreationTime(InstanceID* instance_id);
  InstanceID::Result DeleteID(InstanceID* instance_id);
  std::string GetToken(InstanceID* instance_id,
                       const std::string& authorized_entity,
                       const std::string& scope);
  InstanceID::Result DeleteToken(
      InstanceID* instance_id,
      const std::string& authorized_entity,
      const std::string& scope);

  InstanceIDDriver* driver() const { return driver_.get(); }

 private:
  void GetIDCompleted(const std::string& id);
  void GetCreationTimeCompleted(const base::Time& creation_time);
  void DeleteIDCompleted(InstanceID::Result result);
  void GetTokenCompleted(const std::string& token, InstanceID::Result result);
  void DeleteTokenCompleted(InstanceID::Result result);

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FakeGCMDriverForInstanceID> gcm_driver_;
  std::unique_ptr<InstanceIDDriver> driver_;

#if BUILDFLAG(USE_GCM_FROM_PLATFORM)
  InstanceIDAndroid::ScopedBlockOnAsyncTasksForTesting block_async_;
  ScopedUseFakeInstanceIDAndroid use_fake_;
#endif  // BUILDFLAG(USE_GCM_FROM_PLATFORM)

  std::string id_;
  base::Time creation_time_;
  std::string token_;
  InstanceID::Result result_;

  bool async_operation_completed_;
  base::OnceClosure async_operation_completed_callback_;
};

InstanceIDDriverTest::InstanceIDDriverTest()
    : task_environment_(
          base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
      result_(InstanceID::UNKNOWN_ERROR),
      async_operation_completed_(false) {}

InstanceIDDriverTest::~InstanceIDDriverTest() {
}

void InstanceIDDriverTest::SetUp() {
  gcm_driver_ = std::make_unique<FakeGCMDriverForInstanceID>();
  RecreateInstanceIDDriver();
}

void InstanceIDDriverTest::TearDown() {
  driver_.reset();
  gcm_driver_.reset();
  // |gcm_driver_| owns a GCMKeyStore that owns a ProtoDatabase whose
  // destructor deletes the underlying LevelDB on the task runner.
  base::RunLoop().RunUntilIdle();
}

void InstanceIDDriverTest::RecreateInstanceIDDriver() {
  driver_ = std::make_unique<InstanceIDDriver>(gcm_driver_.get());
}

void InstanceIDDriverTest::WaitForAsyncOperation() {
  // No need to wait if async operation is not needed.
  if (async_operation_completed_)
    return;
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

std::string InstanceIDDriverTest::GetID(InstanceID* instance_id) {
  async_operation_completed_ = false;
  id_.clear();
  instance_id->GetID(base::BindOnce(&InstanceIDDriverTest::GetIDCompleted,
                                    base::Unretained(this)));
  WaitForAsyncOperation();
  return id_;
}

base::Time InstanceIDDriverTest::GetCreationTime(InstanceID* instance_id) {
  async_operation_completed_ = false;
  creation_time_ = base::Time();
  instance_id->GetCreationTime(base::BindOnce(
      &InstanceIDDriverTest::GetCreationTimeCompleted, base::Unretained(this)));
  WaitForAsyncOperation();
  return creation_time_;
}

InstanceID::Result InstanceIDDriverTest::DeleteID(InstanceID* instance_id) {
  async_operation_completed_ = false;
  result_ = InstanceID::UNKNOWN_ERROR;
  instance_id->DeleteID(base::BindOnce(&InstanceIDDriverTest::DeleteIDCompleted,
                                       base::Unretained(this)));
  WaitForAsyncOperation();
  return result_;
}

std::string InstanceIDDriverTest::GetToken(InstanceID* instance_id,
                                           const std::string& authorized_entity,
                                           const std::string& scope) {
  async_operation_completed_ = false;
  token_.clear();
  result_ = InstanceID::UNKNOWN_ERROR;
  instance_id->GetToken(
      authorized_entity, scope, /*time_to_live=*/base::TimeDelta(),
      /*flags=*/{},
      base::BindRepeating(&InstanceIDDriverTest::GetTokenCompleted,
                          base::Unretained(this)));
  WaitForAsyncOperation();
  return token_;
}

InstanceID::Result InstanceIDDriverTest::DeleteToken(
    InstanceID* instance_id,
    const std::string& authorized_entity,
    const std::string& scope) {
  async_operation_completed_ = false;
  result_ = InstanceID::UNKNOWN_ERROR;
  instance_id->DeleteToken(
      authorized_entity, scope,
      base::BindOnce(&InstanceIDDriverTest::DeleteTokenCompleted,
                     base::Unretained(this)));
  WaitForAsyncOperation();
  return result_;
}

void InstanceIDDriverTest::GetIDCompleted(const std::string& id) {
  DCHECK(!async_operation_completed_);
  async_operation_completed_ = true;
  id_ = id;
  if (async_operation_completed_callback_)
    std::move(async_operation_completed_callback_).Run();
}

void InstanceIDDriverTest::GetCreationTimeCompleted(
    const base::Time& creation_time) {
  DCHECK(!async_operation_completed_);
  async_operation_completed_ = true;
  creation_time_ = creation_time;
  if (async_operation_completed_callback_)
    std::move(async_operation_completed_callback_).Run();
}

void InstanceIDDriverTest::DeleteIDCompleted(InstanceID::Result result) {
  DCHECK(!async_operation_completed_);
  async_operation_completed_ = true;
  result_ = result;
  if (async_operation_completed_callback_)
    std::move(async_operation_completed_callback_).Run();
}

void InstanceIDDriverTest::GetTokenCompleted(
    const std::string& token, InstanceID::Result result) {
  DCHECK(!async_operation_completed_);
  async_operation_completed_ = true;
  token_ = token;
  result_ = result;
  if (async_operation_completed_callback_)
    std::move(async_operation_completed_callback_).Run();
}

void InstanceIDDriverTest::DeleteTokenCompleted(InstanceID::Result result) {
  DCHECK(!async_operation_completed_);
  async_operation_completed_ = true;
  result_ = result;
  if (async_operation_completed_callback_)
    std::move(async_operation_completed_callback_).Run();
}

TEST_F(InstanceIDDriverTest, GetAndRemoveInstanceID) {
  EXPECT_FALSE(driver()->ExistsInstanceID(kTestAppID1));

  InstanceID* instance_id = driver()->GetInstanceID(kTestAppID1);
  EXPECT_TRUE(instance_id);
  EXPECT_TRUE(driver()->ExistsInstanceID(kTestAppID1));

  driver()->RemoveInstanceID(kTestAppID1);
  EXPECT_FALSE(driver()->ExistsInstanceID(kTestAppID1));
}

TEST_F(InstanceIDDriverTest, NewID) {
  // Creation time should not be set when the ID is not created.
  InstanceID* instance_id1 = driver()->GetInstanceID(kTestAppID1);
  EXPECT_TRUE(GetCreationTime(instance_id1).is_null());

  // New ID is generated for the first time.
  std::string id1 = GetID(instance_id1);
  EXPECT_TRUE(VerifyInstanceID(id1));
  base::Time creation_time = GetCreationTime(instance_id1);
  EXPECT_FALSE(creation_time.is_null());

  // Same ID is returned for the same app.
  EXPECT_EQ(id1, GetID(instance_id1));
  EXPECT_EQ(creation_time, GetCreationTime(instance_id1));

  // New ID is generated for another app.
  InstanceID* instance_id2 = driver()->GetInstanceID(kTestAppID2);
  std::string id2 = GetID(instance_id2);
  EXPECT_TRUE(VerifyInstanceID(id2));
  EXPECT_NE(id1, id2);
  EXPECT_FALSE(GetCreationTime(instance_id2).is_null());
}

TEST_F(InstanceIDDriverTest, PersistID) {
  InstanceID* instance_id = driver()->GetInstanceID(kTestAppID1);

  // Create the ID for the first time. The ID and creation time should be saved
  // to the store.
  std::string id = GetID(instance_id);
  EXPECT_FALSE(id.empty());
  base::Time creation_time = GetCreationTime(instance_id);
  EXPECT_FALSE(creation_time.is_null());

  // Simulate restart by recreating InstanceIDDriver. Same ID and creation time
  // should be expected.
  RecreateInstanceIDDriver();
  instance_id = driver()->GetInstanceID(kTestAppID1);
  EXPECT_EQ(creation_time, GetCreationTime(instance_id));
  EXPECT_EQ(id, GetID(instance_id));

  // Delete the ID. The ID and creation time should be removed from the store.
  EXPECT_EQ(InstanceID::SUCCESS, DeleteID(instance_id));
  EXPECT_TRUE(GetCreationTime(instance_id).is_null());

  // Simulate restart by recreating InstanceIDDriver. Different ID should be
  // expected.
  // Note that we do not check for different creation time since the test might
  // be run at a very fast server.
  RecreateInstanceIDDriver();
  instance_id = driver()->GetInstanceID(kTestAppID1);
  EXPECT_NE(id, GetID(instance_id));
}

TEST_F(InstanceIDDriverTest, DeleteID) {
  InstanceID* instance_id = driver()->GetInstanceID(kTestAppID1);
  std::string id1 = GetID(instance_id);
  EXPECT_FALSE(id1.empty());
  EXPECT_FALSE(GetCreationTime(instance_id).is_null());

  // New ID will be generated from GetID after calling DeleteID.
  EXPECT_EQ(InstanceID::SUCCESS, DeleteID(instance_id));
  EXPECT_TRUE(GetCreationTime(instance_id).is_null());

  std::string id2 = GetID(instance_id);
  EXPECT_FALSE(id2.empty());
  EXPECT_NE(id1, id2);
  EXPECT_FALSE(GetCreationTime(instance_id).is_null());
}

TEST_F(InstanceIDDriverTest, GetToken) {
  InstanceID* instance_id = driver()->GetInstanceID(kTestAppID1);
  std::string token1 = GetToken(instance_id, kAuthorizedEntity1, kScope1);
  EXPECT_FALSE(token1.empty());

  // Same token is returned for same authorized entity and scope.
  EXPECT_EQ(token1, GetToken(instance_id, kAuthorizedEntity1, kScope1));

  // Different token is returned for different authorized entity or scope.
  std::string token2 = GetToken(instance_id, kAuthorizedEntity1, kScope2);
  EXPECT_FALSE(token2.empty());
  EXPECT_NE(token1, token2);

  std::string token3 = GetToken(instance_id, kAuthorizedEntity2, kScope1);
  EXPECT_FALSE(token3.empty());
  EXPECT_NE(token1, token3);
  EXPECT_NE(token2, token3);
}

TEST_F(InstanceIDDriverTest, DeleteToken) {
  InstanceID* instance_id = driver()->GetInstanceID(kTestAppID1);

  // Gets 2 tokens.
  std::string token1 = GetToken(instance_id, kAuthorizedEntity1, kScope1);
  EXPECT_FALSE(token1.empty());
  std::string token2 = GetToken(instance_id, kAuthorizedEntity2, kScope1);
  EXPECT_FALSE(token1.empty());
  EXPECT_NE(token1, token2);

  // Different token is returned for same authorized entity and scope after
  // deletion.
  EXPECT_EQ(InstanceID::SUCCESS,
            DeleteToken(instance_id, kAuthorizedEntity1, kScope1));
  std::string new_token1 = GetToken(instance_id, kAuthorizedEntity1, kScope2);
  EXPECT_FALSE(new_token1.empty());
  EXPECT_NE(token1, new_token1);

  // The other token is not affected by the deletion.
  EXPECT_EQ(token2, GetToken(instance_id, kAuthorizedEntity2, kScope1));
}

}  // namespace instance_id
