// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;

// Converts a string to an array for printing.
std::vector<int> StringToArray(const std::string& s) {
  return std::vector<int>(s.begin(), s.end());
}

}  // namespace

TEST(LeakDetectionRequestUtils, PrepareSingleLeakRequestData) {
  base::test::TaskEnvironment task_env;
  base::MockCallback<SingleLeakRequestDataCallback> callback;

  PrepareSingleLeakRequestData("jonsnow", "1234", callback.Get());
  EXPECT_CALL(
      callback,
      Run(AllOf(
          Field(&LookupSingleLeakData::username_hash_prefix,
                ElementsAre(61, 112, -45)),
          Field(&LookupSingleLeakData::encrypted_payload, testing::Ne("")),
          Field(&LookupSingleLeakData::encryption_key, testing::Ne("")))));
  task_env.RunUntilIdle();
}

TEST(LeakDetectionRequestUtils, AnalyzeResponseResult_DecryptionError) {
  base::test::TaskEnvironment task_env;

  // Force a decryption error by returning trash bytes.
  auto response = std::make_unique<SingleLookupResponse>();
  response->reencrypted_lookup_hash = "trash_bytes";

  base::MockCallback<SingleLeakResponseAnalysisCallback> callback;
  AnalyzeResponse(std::move(response), "random_key", callback.Get());
  EXPECT_CALL(callback, Run(AnalyzeResponseResult::kDecryptionError));
  task_env.RunUntilIdle();
}

TEST(LeakDetectionRequestUtils, AnalyzeResponseResult_NoLeak) {
  base::test::TaskEnvironment task_env;

  constexpr char kUsernamePasswordHash[] = "abcdefg";
  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_client;
  std::string encrypted_username_password =
      CipherEncrypt(kUsernamePasswordHash, &key_client);
  std::string key_server;
  response->reencrypted_lookup_hash =
      CipherReEncrypt(encrypted_username_password, &key_server);
  SCOPED_TRACE(testing::Message()
               << "key_client="
               << testing::PrintToString(StringToArray(key_client))
               << ", key_server="
               << testing::PrintToString(StringToArray(key_server)));

  response->encrypted_leak_match_prefixes.push_back(crypto::SHA256HashString(
      CipherEncryptWithKey("unrelated_trash", key_server)));

  base::MockCallback<SingleLeakResponseAnalysisCallback> callback;
  AnalyzeResponse(std::move(response), key_client, callback.Get());
  EXPECT_CALL(callback, Run(AnalyzeResponseResult::kNotLeaked));
  task_env.RunUntilIdle();
}

TEST(LeakDetectionRequestUtils, AnalyzeResponseResult_Leak) {
  base::test::TaskEnvironment task_env;

  constexpr char kUsernamePasswordHash[] = "abcdefg";
  auto response = std::make_unique<SingleLookupResponse>();
  std::string key_client;
  std::string encrypted_username_password =
      CipherEncrypt(kUsernamePasswordHash, &key_client);
  std::string key_server;
  response->reencrypted_lookup_hash =
      CipherReEncrypt(encrypted_username_password, &key_server);
  SCOPED_TRACE(testing::Message()
               << "key_client="
               << testing::PrintToString(StringToArray(key_client))
               << ", key_server="
               << testing::PrintToString(StringToArray(key_server)));

  // Random length of the prefix for values in |encrypted_leak_match_prefixes|.
  // The server can pick any value.
  constexpr int kPrefixLength = 30;
  response->encrypted_leak_match_prefixes.push_back(crypto::SHA256HashString(
      CipherEncryptWithKey("unrelated_trash", key_server)));
  response->encrypted_leak_match_prefixes.push_back(
      crypto::SHA256HashString(
          CipherEncryptWithKey(kUsernamePasswordHash, key_server))
          .substr(0, kPrefixLength));

  base::MockCallback<SingleLeakResponseAnalysisCallback> callback;

  AnalyzeResponse(std::move(response), key_client, callback.Get());
  EXPECT_CALL(callback, Run(AnalyzeResponseResult::kLeaked));
  task_env.RunUntilIdle();
}

}  // namespace password_manager
