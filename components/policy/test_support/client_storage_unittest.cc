// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/client_storage.h"

#include <string_view>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr const char kDeviceId1[] = "1";
constexpr const char kDeviceId2[] = "2";
constexpr const char kStateKey1[] = "bbb";
constexpr const char kStateKey2[] = "ggg";
constexpr const char kStateKey3[] = "fff";
constexpr const char kStateKey4[] = "ccc";
constexpr const char kDeviceToken[] = "device-token";
constexpr const char kNonExistingDeviceToken[] = "non-existing-device-token";
constexpr const uint64_t kModulus = 3;
constexpr const uint64_t kRemainder = 2;
// Following SHA256 hashes produce |kRemainder| when divided by |kModulus|.
constexpr std::string_view kSHA256HashForStateKey1(
    "\x3e\x74\x4b\x9d\xc3\x93\x89\xba\xf0\xc5\xa0\x66\x05\x89\xb8\x40\x2f\x3d"
    "\xbb\x49\xb8\x9b\x3e\x75\xf2\xc9\x35\x58\x52\xa3\xc6\x77",
    32);
constexpr std::string_view kSHA256HashForStateKey4(
    "\x64\xda\xa4\x4a\xd4\x93\xff\x28\xa9\x6e\xff\xab\x6e\x77\xf1\x73\x2a\x3d"
    "\x97\xd8\x32\x41\x58\x1b\x37\xdb\xd7\x0a\x7a\x49\x00\xfe",
    32);

void RegisterClient(const std::string& device_token,
                    ClientStorage* client_storage) {
  ClientStorage::ClientInfo client_info;
  client_info.device_id = kDeviceId1;
  client_info.device_token = device_token;

  client_storage->RegisterClient(client_info);
  ASSERT_EQ(client_storage->GetNumberOfRegisteredClients(), 1u);
  ASSERT_EQ(client_storage->GetClient(kDeviceId1).device_token, device_token);
}

}  // namespace

TEST(ClientStorageTest, Unregister_Success) {
  ClientStorage client_storage;
  RegisterClient(kDeviceToken, &client_storage);

  ASSERT_TRUE(client_storage.DeleteClient(kDeviceToken));
  EXPECT_EQ(client_storage.GetNumberOfRegisteredClients(), 0u);
}

TEST(ClientStorageTest, Unregister_NonExistingClient) {
  ClientStorage client_storage;
  RegisterClient(kDeviceToken, &client_storage);

  ASSERT_FALSE(client_storage.DeleteClient(kNonExistingDeviceToken));
  ASSERT_EQ(client_storage.GetNumberOfRegisteredClients(), 1u);
  EXPECT_EQ(client_storage.GetClient(kDeviceId1).device_token, kDeviceToken);
}

TEST(ClientStorageTest, GetMatchingStateKeyHashes) {
  ClientStorage client_storage;
  ClientStorage::ClientInfo client_info1;
  client_info1.device_id = kDeviceId1;
  client_info1.state_keys = {kStateKey1, kStateKey2};
  client_storage.RegisterClient(client_info1);
  ClientStorage::ClientInfo client_info2;
  client_info2.device_id = kDeviceId2;
  client_info2.state_keys = {kStateKey3, kStateKey4};
  client_storage.RegisterClient(client_info2);

  std::vector<std::string> matching_hashes =
      client_storage.GetMatchingStateKeyHashes(kModulus, kRemainder);

  EXPECT_THAT(matching_hashes,
              testing::UnorderedElementsAreArray(
                  {kSHA256HashForStateKey1, kSHA256HashForStateKey4}));
}

}  // namespace policy
