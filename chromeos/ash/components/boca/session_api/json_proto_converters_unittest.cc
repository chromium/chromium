// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/json_proto_converters.h"

#include <string>
#include <string_view>

#include "base/values.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

constexpr std::string_view kEmailValue = "test@example.com";
constexpr std::string_view kGaiaIdValue = "gaiaId";
constexpr std::string_view kFullNameValue = "fullName";
constexpr std::string_view kPhotoUrlValue = "photoUrl";

TEST(JsonProtoConvertersTest, ConvertUserIdentityJsonToProto) {
  base::Value::Dict dict;
  dict.Set(kEmail, kEmailValue);
  dict.Set(kGaiaId, kGaiaIdValue);
  dict.Set(kFullName, kFullNameValue);
  dict.Set(kPhotoUrl, kPhotoUrlValue);

  ::boca::UserIdentity user_identity = ConvertUserIdentityJsonToProto(&dict);

  EXPECT_EQ(user_identity.email(), kEmailValue);
  EXPECT_EQ(user_identity.gaia_id(), kGaiaIdValue);
  EXPECT_EQ(user_identity.full_name(), kFullNameValue);
  EXPECT_EQ(user_identity.photo_url(), kPhotoUrlValue);
}

}  // namespace
}  // namespace ash::boca
