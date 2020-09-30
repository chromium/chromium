// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/privacy_budget/field_trial_param_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(FieldTrialParamConversionsTest, Surface) {
  auto original_surface = blink::IdentifiableSurface::FromMetricHash(100);
  EXPECT_EQ(std::string("100"), EncodeIdentifiabilityType(original_surface));

  original_surface = blink::IdentifiableSurface::FromTypeAndToken(
      blink::IdentifiableSurface::Type::kWebFeature, 1);
  std::string encoded = EncodeIdentifiabilityType(original_surface);
  EXPECT_EQ(std::string("257"), encoded);

  auto decoded_surface = blink::IdentifiableSurface();
  EXPECT_TRUE(DecodeIdentifiabilityType(encoded, &decoded_surface));
  EXPECT_EQ(original_surface, decoded_surface);
}

TEST(FieldTrialParamConversionsTest, Type) {
  auto original_type = blink::IdentifiableSurface::Type::kWebFeature;
  std::string encoded = EncodeIdentifiabilityType(original_type);
  EXPECT_EQ(std::string("1"), encoded);

  auto foo = blink::IdentifiableSurface::Type::kReservedInternal;
  EXPECT_TRUE(DecodeIdentifiabilityType(encoded, &foo));
  EXPECT_EQ(original_type, foo);
}

TEST(FieldTrialParamConversionsTest, FieldTrialParam_Surface) {
  std::map<blink::IdentifiableSurface, int> original_map;
  original_map[blink::IdentifiableSurface::FromMetricHash(100)] = 5;
  original_map[blink::IdentifiableSurface::FromMetricHash(UINT64_C(1) << 55)] =
      5;

  auto encoded = EncodeIdentifiabilityFieldTrialParam(original_map);
  EXPECT_EQ(std::string("100;5,36028797018963968;5"), encoded);

  auto decoded_map = DecodeIdentifiabilityFieldTrialParam<
      std::map<blink::IdentifiableSurface, int>>(encoded);
  EXPECT_EQ(original_map, decoded_map);
}

TEST(FieldTrialParamConversionsTest, FieldTrialParam_Type) {
  std::map<blink::IdentifiableSurface::Type, int> original_map;
  original_map[blink::IdentifiableSurface::Type::kReservedInternal] = 6;
  original_map[blink::IdentifiableSurface::Type::kWebFeature] = 5;

  auto encoded = EncodeIdentifiabilityFieldTrialParam(original_map);
  EXPECT_EQ(std::string("0;6,1;5"), encoded);

  auto decoded_map = DecodeIdentifiabilityFieldTrialParam<
      std::map<blink::IdentifiableSurface::Type, int>>(encoded);
  EXPECT_EQ(original_map, decoded_map);
}

TEST(FieldTrialParamConversionsTest, DecodeBadSurface) {
  auto decoded_surface = blink::IdentifiableSurface();
  EXPECT_FALSE(DecodeIdentifiabilityType("foo", &decoded_surface));
  EXPECT_FALSE(DecodeIdentifiabilityType("-100", &decoded_surface));
  EXPECT_FALSE(DecodeIdentifiabilityType("100000000000000000000000000",
                                         &decoded_surface));
}

TEST(FieldTrialParamConversionsTest, DecodeBadType) {
  auto decoded_type = blink::IdentifiableSurface::Type::kReservedInternal;
  EXPECT_FALSE(DecodeIdentifiabilityType("foo", &decoded_type));
  EXPECT_FALSE(DecodeIdentifiabilityType("-100", &decoded_type));
  EXPECT_FALSE(DecodeIdentifiabilityType("256", &decoded_type));
}
