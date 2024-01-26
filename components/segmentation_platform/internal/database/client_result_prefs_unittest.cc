// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/client_result_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

const char kClientKey[] = "test_key";
const char kClientKey2[] = "test_key2";

class ClientResultPrefsTest : public testing::Test {
 public:
  ClientResultPrefsTest() = default;
  ~ClientResultPrefsTest() override = default;

  void SetUp() override {
    result_prefs_ = std::make_unique<ClientResultPrefs>(&pref_service_);
    pref_service_.registry()->RegisterStringPref(kSegmentationClientResultPrefs,
                                                 std::string());
  }

  proto::ClientResult CreateClientResult(std::vector<float> result) {
    proto::ClientResult client_result;
    auto* pred_result = client_result.mutable_client_result();
    pred_result->mutable_result()->Add(result.begin(), result.end());
    client_result.set_timestamp_us(
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
    return client_result;
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClientResultPrefs> result_prefs_;
};

TEST_F(ClientResultPrefsTest, ReadClientResultFromEmptyPrefs) {
  const proto::ClientResult* current_result =
      result_prefs_->ReadClientResultFromPrefs(kClientKey);
  EXPECT_FALSE(current_result);
}

TEST_F(ClientResultPrefsTest, SaveClientResultToEmptyPrefs) {
  const proto::ClientResult& client_result = CreateClientResult({0.8});
  result_prefs_->SaveClientResultToPrefs(kClientKey, client_result);

  const proto::ClientResult* current_result =
      result_prefs_->ReadClientResultFromPrefs(kClientKey);
  EXPECT_TRUE(current_result);
  EXPECT_EQ(client_result.SerializeAsString(),
            current_result->SerializeAsString());
}

TEST_F(ClientResultPrefsTest, SaveMultipleClientResults) {
  // Saving multiple keys and reading multiple keys.
  const proto::ClientResult& client_result = CreateClientResult({0.8});
  result_prefs_->SaveClientResultToPrefs(kClientKey, client_result);

  const proto::ClientResult& client_result2 = CreateClientResult({0.7, 0.9});
  result_prefs_->SaveClientResultToPrefs(kClientKey2, client_result2);

  const proto::ClientResult* current_result =
      result_prefs_->ReadClientResultFromPrefs(kClientKey);
  EXPECT_TRUE(current_result);
  EXPECT_EQ(client_result.SerializeAsString(),
            current_result->SerializeAsString());

  current_result = result_prefs_->ReadClientResultFromPrefs(kClientKey2);
  EXPECT_TRUE(current_result);
  EXPECT_EQ(client_result2.SerializeAsString(),
            current_result->SerializeAsString());

  // Save empty result. It should delete the current result.
  result_prefs_->SaveClientResultToPrefs(kClientKey2, std::nullopt);
  current_result = result_prefs_->ReadClientResultFromPrefs(kClientKey2);
  EXPECT_FALSE(current_result);

  current_result = result_prefs_->ReadClientResultFromPrefs(kClientKey);
  EXPECT_TRUE(current_result);
  EXPECT_EQ(client_result.SerializeAsString(),
            current_result->SerializeAsString());

  // Updating client result for `kClientKey2`. It should overwrite the
  // result.
  const proto::ClientResult& client_result3 = CreateClientResult({});
  result_prefs_->SaveClientResultToPrefs(kClientKey2, client_result3);
  current_result = result_prefs_->ReadClientResultFromPrefs(kClientKey2);
  EXPECT_TRUE(current_result);
  EXPECT_EQ(client_result3.SerializeAsString(),
            current_result->SerializeAsString());

  current_result = result_prefs_->ReadClientResultFromPrefs(kClientKey);
  EXPECT_TRUE(current_result);
  EXPECT_EQ(client_result.SerializeAsString(),
            current_result->SerializeAsString());
}

}  // namespace segmentation_platform
