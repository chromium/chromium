// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/policy_storage.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr const char kFakePolicyType[] = "fake_policy_type";
constexpr const char kFakeEntityId[] = "fake_entity_id";
constexpr const char kRawPolicyPayload[] = R"({"foo": "bar"})";

}  // namespace

TEST(PolicyStorageTest, StoresExternalPolicyData) {
  PolicyStorage policy_storage;
  policy_storage.SetExternalPolicyPayload(kFakePolicyType, kFakeEntityId,
                                          kRawPolicyPayload);

  EXPECT_EQ(
      policy_storage.GetExternalPolicyPayload(kFakePolicyType, kFakeEntityId),
      kRawPolicyPayload);
  // Check that external policy payloads are stored separately from regular
  // policy payloads.
  EXPECT_THAT(policy_storage.GetPolicyPayload(kFakePolicyType, kFakeEntityId),
              testing::IsEmpty());
}

}  // namespace policy
