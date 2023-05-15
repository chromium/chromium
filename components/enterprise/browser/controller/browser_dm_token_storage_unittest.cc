// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/controller/browser_dm_token_storage.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;

namespace policy {

namespace {

constexpr char kClientId1[] = "fake-client-id-1";
constexpr char kClientId2[] = "fake-client-id-2";
// client id with more than 64 characters (this has 65)
constexpr char kClientIdTooLong[] =
    "r9ilXvBi0pXagvaAAGFexok99FJ0Kjkzz4Yu6YmkxyIJDn6uxDgTGTNSdBfpjJZPI";

constexpr char kEnrollmentToken1[] = "fake-enrollment-token-1";
constexpr char kEnrollmentToken2[] = "fake-enrollment-token-2";
constexpr char kDMToken1[] = "fake-dm-token-1";
constexpr char kDMToken2[] = "fake-dm-token-2";

class BrowserDMTokenStorageTestBase {
 public:
  BrowserDMTokenStorageTestBase(const std::string& client_id,
                                const std::string& enrollment_token,
                                const std::string& dm_token,
                                const bool enrollment_error_option)
      : storage_(client_id,
                 enrollment_token,
                 dm_token,
                 enrollment_error_option) {}
  FakeBrowserDMTokenStorage storage_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class BrowserDMTokenStorageTest : public BrowserDMTokenStorageTestBase,
                                  public testing::Test {
 public:
  BrowserDMTokenStorageTest()
      : BrowserDMTokenStorageTestBase(kClientId1,
                                      kEnrollmentToken1,
                                      kDMToken1,
                                      false) {}
};

struct StoreAndRetrieveTestParams {
 public:
  StoreAndRetrieveTestParams(const std::string& dm_token_to_store,
                             const DMToken& expected_retrieved_dm_token)
      : dm_token_to_store(dm_token_to_store),
        expected_retrieved_dm_token(expected_retrieved_dm_token) {}

  std::string dm_token_to_store;
  const DMToken expected_retrieved_dm_token;
};

class BrowserDMTokenStorageStoreAndRetrieveTest
    : public BrowserDMTokenStorageTestBase,
      public testing::TestWithParam<StoreAndRetrieveTestParams> {
 public:
  BrowserDMTokenStorageStoreAndRetrieveTest()
      : BrowserDMTokenStorageTestBase(kClientId1,
                                      kEnrollmentToken1,
                                      GetParam().dm_token_to_store,
                                      false) {}
  DMToken GetExpectedToken() { return GetParam().expected_retrieved_dm_token; }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    BrowserDMTokenStorageStoreAndRetrieveTest,
    BrowserDMTokenStorageStoreAndRetrieveTest,
    testing::Values(
        StoreAndRetrieveTestParams(kDMToken1,
                                   DMToken::CreateValidToken(kDMToken1)),
        StoreAndRetrieveTestParams(kDMToken2,
                                   DMToken::CreateValidToken(kDMToken2)),
        StoreAndRetrieveTestParams("INVALID_DM_TOKEN",
                                   DMToken::CreateInvalidToken()),
        StoreAndRetrieveTestParams("", DMToken::CreateEmptyToken())));

TEST_F(BrowserDMTokenStorageTest, RetrieveClientId) {
  EXPECT_EQ(kClientId1, storage_.RetrieveClientId());
  // The client ID value should be cached in memory and not read from the system
  // again.
  storage_.SetClientId(kClientId2);
  EXPECT_EQ(kClientId1, storage_.RetrieveClientId());
}

TEST_F(BrowserDMTokenStorageTest, RetrieveEnrollmentToken) {
  EXPECT_EQ(kEnrollmentToken1, storage_.RetrieveEnrollmentToken());

  // The enrollment token should be cached in memory and not read from the
  // system again.
  storage_.SetEnrollmentToken(kEnrollmentToken2);
  EXPECT_EQ(kEnrollmentToken1, storage_.RetrieveEnrollmentToken());
}

TEST_P(BrowserDMTokenStorageStoreAndRetrieveTest, StoreDMToken) {
  storage_.SetDMToken(GetParam().dm_token_to_store);
  DMToken dm_token = storage_.RetrieveDMToken();
  if (GetExpectedToken().is_valid()) {
    EXPECT_EQ(GetExpectedToken().value(), dm_token.value());
  }
  EXPECT_EQ(GetExpectedToken().is_valid(), dm_token.is_valid());
  EXPECT_EQ(GetExpectedToken().is_invalid(), dm_token.is_invalid());
  EXPECT_EQ(GetExpectedToken().is_empty(), dm_token.is_empty());

  // The DM token should be cached in memory and not read from the system again.
  storage_.SetDMToken("not_saved");
  dm_token = storage_.RetrieveDMToken();
  EXPECT_EQ(GetExpectedToken().is_valid(), dm_token.is_valid());
  EXPECT_EQ(GetExpectedToken().is_invalid(), dm_token.is_invalid());
  EXPECT_EQ(GetExpectedToken().is_empty(), dm_token.is_empty());
  if (GetExpectedToken().is_valid()) {
    EXPECT_EQ(GetExpectedToken().value(), dm_token.value());
  }
}

TEST_F(BrowserDMTokenStorageTest, InvalidateDMToken) {
  storage_.SetDMToken(kDMToken1);
  DMToken initial_dm_token = storage_.RetrieveDMToken();
  EXPECT_EQ(kDMToken1, initial_dm_token.value());
  EXPECT_TRUE(initial_dm_token.is_valid());

  storage_.InvalidateDMToken(base::DoNothing());
  DMToken invalid_dm_token = storage_.RetrieveDMToken();
  EXPECT_TRUE(invalid_dm_token.is_invalid());
}

TEST_F(BrowserDMTokenStorageTest, ClearDMToken) {
  storage_.SetDMToken(kDMToken1);
  DMToken initial_dm_token = storage_.RetrieveDMToken();
  EXPECT_EQ(kDMToken1, initial_dm_token.value());
  EXPECT_TRUE(initial_dm_token.is_valid());

  storage_.ClearDMToken(base::DoNothing());
  DMToken empty_dm_token = storage_.RetrieveDMToken();
  EXPECT_TRUE(empty_dm_token.is_empty());
}

TEST_P(BrowserDMTokenStorageStoreAndRetrieveTest, RetrieveDMToken) {
  DMToken dm_token = storage_.RetrieveDMToken();
  if (GetExpectedToken().is_valid()) {
    EXPECT_EQ(GetExpectedToken().value(), dm_token.value());
  }
  EXPECT_EQ(GetExpectedToken().is_valid(), dm_token.is_valid());
  EXPECT_EQ(GetExpectedToken().is_invalid(), dm_token.is_invalid());
  EXPECT_EQ(GetExpectedToken().is_empty(), dm_token.is_empty());
}

TEST_F(BrowserDMTokenStorageTest, ShouldDisplayErrorMessageOnFailure) {
  EXPECT_FALSE(storage_.ShouldDisplayErrorMessageOnFailure());

  // The error option should be cached in memory and not read from the system
  // again.
  storage_.SetEnrollmentErrorOption(true);
  EXPECT_FALSE(storage_.ShouldDisplayErrorMessageOnFailure());
}

TEST_F(BrowserDMTokenStorageTest, SetDelegate) {
  // The initial delegate has already been set by the test fixture ctor. This
  // next call should not modify the existing delegate.
  BrowserDMTokenStorage::SetDelegate(
      std::make_unique<FakeBrowserDMTokenStorage::MockDelegate>(
          kClientId2, kEnrollmentToken2, kDMToken2, true));

  // Make sure the initial delegate is the one reading the values.
  EXPECT_EQ(storage_.RetrieveClientId(), kClientId1);
  EXPECT_EQ(storage_.RetrieveEnrollmentToken(), kEnrollmentToken1);
  EXPECT_EQ(storage_.RetrieveDMToken().value(), kDMToken1);
  EXPECT_EQ(storage_.ShouldDisplayErrorMessageOnFailure(), false);
}

TEST_F(BrowserDMTokenStorageTest, InvalidClientId) {
  FakeBrowserDMTokenStorage newStorage;
  newStorage.SetClientId("id with spaces");
  EXPECT_EQ(newStorage.RetrieveClientId(), "");

  newStorage.SetClientId("id-invalid\n");
  EXPECT_EQ(newStorage.RetrieveClientId(), "");
}

TEST_F(BrowserDMTokenStorageTest, ClientIdTooLong) {
  FakeBrowserDMTokenStorage newStorage;
  newStorage.SetClientId(kClientIdTooLong);
  EXPECT_EQ(newStorage.RetrieveClientId(), "");
}

}  // namespace policy
