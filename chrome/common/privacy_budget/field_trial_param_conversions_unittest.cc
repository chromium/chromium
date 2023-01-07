// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/privacy_budget/field_trial_param_conversions.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "chrome/common/privacy_budget/types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace privacy_budget_internal {

namespace {

// 100
constexpr auto kSurface1 = blink::IdentifiableSurface::FromMetricHash(100);

// 257
constexpr auto kSurface2 = blink::IdentifiableSurface::FromTypeAndToken(
    blink::IdentifiableSurface::Type::kWebFeature,
    1);

// 36028797018963968
constexpr auto kSurface3 =
    blink::IdentifiableSurface::FromMetricHash(UINT64_C(1) << 55);

// 1
constexpr auto kType1 = blink::IdentifiableSurface::Type::kWebFeature;

// 9
constexpr auto kType2 =
    blink::IdentifiableSurface::Type::kMediaRecorder_IsTypeSupported;

}  // namespace

TEST(FieldTrialParamConversionsTest, EncodeDecodeSingleSurface) {
  EXPECT_EQ(std::string("100"), EncodeIdentifiabilityType(kSurface1));

  auto decoded_surface = blink::IdentifiableSurface();
  EXPECT_TRUE(DecodeIdentifiabilityType("257", &decoded_surface));
  EXPECT_EQ(kSurface2, decoded_surface);

  EXPECT_FALSE(DecodeIdentifiabilityType("", &decoded_surface));
}

TEST(FieldTrialParamConversionsTest, EncodeDecodeSingleType) {
  EXPECT_EQ(std::string("1"), EncodeIdentifiabilityType(kType1));

  auto foo = blink::IdentifiableSurface::Type::kReservedInternal;
  EXPECT_TRUE(DecodeIdentifiabilityType("1", &foo));
  EXPECT_EQ(kType1, foo);

  EXPECT_FALSE(DecodeIdentifiabilityType("", &foo));
}

TEST(FieldTrialParamConversionsTest, SurfaceToIntMap) {
  const std::map<blink::IdentifiableSurface, unsigned int> original_map = {
      {kSurface1, 5}, {kSurface3, 5}};

  auto encoded = EncodeIdentifiabilityFieldTrialParam(original_map);
  EXPECT_EQ(std::string("100;5,36028797018963968;5"), encoded);

  auto decoded_map = DecodeIdentifiabilityFieldTrialParam<
      std::map<blink::IdentifiableSurface, unsigned int>>(encoded);
  EXPECT_EQ(original_map, decoded_map);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceList) {
  const IdentifiableSurfaceList kSurfaceList = {kSurface1, kSurface2,
                                                kSurface3};

  auto encoded_surface_list =
      EncodeIdentifiabilityFieldTrialParam(kSurfaceList);
  EXPECT_EQ(std::string("100,257,36028797018963968"), encoded_surface_list);

  auto decoded_surface_list =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceList>(
          encoded_surface_list);
  EXPECT_EQ(kSurfaceList, decoded_surface_list);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceTypeList) {
  const IdentifiableSurfaceTypeList kTypeList = {kType1, kType2};

  auto encoded_type_list = EncodeIdentifiabilityFieldTrialParam(kTypeList);
  EXPECT_EQ(std::string("1,9"), encoded_type_list);

  auto decoded_type_list =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceTypeList>(
          encoded_type_list);
  EXPECT_EQ(kTypeList, decoded_type_list);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceSet) {
  const IdentifiableSurfaceSet kSurfaceSet = {kSurface1, kSurface2, kSurface3};

  auto encoded_surface_set = EncodeIdentifiabilityFieldTrialParam(kSurfaceSet);
  EXPECT_EQ(std::string("100,257,36028797018963968"), encoded_surface_set);

  auto decoded_surface_set =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceSet>(
          encoded_surface_set);
  EXPECT_EQ(kSurfaceSet, decoded_surface_set);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceTypeSet) {
  const IdentifiableSurfaceTypeSet kTypeSet = {kType1, kType2};

  auto encoded_type_set = EncodeIdentifiabilityFieldTrialParam(kTypeSet);
  EXPECT_EQ(std::string("1,9"), encoded_type_set);

  auto decoded_type_set =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceTypeSet>(
          encoded_type_set);
  EXPECT_EQ(kTypeSet, decoded_type_set);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceSampleRateMap) {
  IdentifiableSurfaceSampleRateMap original_map = {{kSurface1, 5},
                                                   {kSurface2, 6}};

  auto encoded = EncodeIdentifiabilityFieldTrialParam(original_map);
  EXPECT_EQ(std::string("100;5,257;6"), encoded);

  auto decoded_map =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceSampleRateMap>(
          encoded);
  EXPECT_EQ(original_map, decoded_map);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceTypeSampleRateMap) {
  const IdentifiableSurfaceTypeSampleRateMap original_map = {{kType1, 6},
                                                             {kType2, 7}};

  auto encoded = EncodeIdentifiabilityFieldTrialParam(original_map);
  EXPECT_EQ(std::string("1;6,9;7"), encoded);

  auto decoded_map = DecodeIdentifiabilityFieldTrialParam<
      IdentifiableSurfaceTypeSampleRateMap>(encoded);
  EXPECT_EQ(original_map, decoded_map);

  // Extraneous bad values should be silently skipped.
  auto decoded_with_noise = DecodeIdentifiabilityFieldTrialParam<
      IdentifiableSurfaceTypeSampleRateMap>("1;6,2;3;4,9;7,10");
  EXPECT_EQ(original_map, decoded_with_noise);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceCostMap) {
  const IdentifiableSurfaceCostMap original_map = {
      {kSurface1, 0.5}, {kSurface2, 0.25}, {kSurface3, 0.4}};

  auto encoded =
      EncodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceCostMap>(
          original_map);
  EXPECT_EQ(std::string("100;0.5,257;0.25,36028797018963968;0.4"), encoded);

  auto decoded =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceCostMap>(
          "100;0.5,257;0.25,36028797018963968;0.4");
  EXPECT_EQ(original_map, decoded);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceTypeCostMap) {
  const IdentifiableSurfaceTypeCostMap original_map = {{kType1, 0.5},
                                                       {kType2, 0.25}};

  auto encoded =
      EncodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceTypeCostMap>(
          original_map);
  EXPECT_EQ(std::string("1;0.5,9;0.25"), encoded);

  auto decoded =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceTypeCostMap>(
          "1;0.5,9;0.25");
  EXPECT_EQ(original_map, decoded);
}

TEST(FieldTrialParamConversionsTest, SurfaceSetEquivalentClassesList) {
  const SurfaceSetEquivalentClassesList original_classes = {
      {kSurface1, kSurface2, kSurface3}, {kSurface3, kSurface2, kSurface1}};
  auto encoded =
      EncodeIdentifiabilityFieldTrialParam<SurfaceSetEquivalentClassesList>(
          original_classes);
  EXPECT_EQ(std::string("100;257;36028797018963968,36028797018963968;257;100"),
            encoded);
  auto decoded =
      DecodeIdentifiabilityFieldTrialParam<SurfaceSetEquivalentClassesList>(
          encoded);
  EXPECT_EQ(original_classes, decoded);
}

TEST(FieldTrialParamConversionsTest, IdentifiableSurfaceBlocks) {
  const IdentifiableSurfaceBlocks original_classes = {
      {kSurface1, kSurface2, kSurface3}, {kSurface3, kSurface2, kSurface1}};
  auto encoded =
      EncodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceBlocks>(
          original_classes);
  EXPECT_EQ(std::string("100;257;36028797018963968,36028797018963968;257;100"),
            encoded);
  auto decoded =
      DecodeIdentifiabilityFieldTrialParam<IdentifiableSurfaceBlocks>(encoded);
  EXPECT_EQ(original_classes, decoded);
}

TEST(FieldTrialParamConversionsTest, VectorOfSizeT) {
  const std::vector<unsigned int> kNumbers = {1, 3, 1000,
                                              std::numeric_limits<int>::max()};
  auto encoded = EncodeIdentifiabilityFieldTrialParam(kNumbers);
  EXPECT_EQ(std::string("1,3,1000,2147483647"), encoded);

  auto decoded =
      DecodeIdentifiabilityFieldTrialParam<std::vector<unsigned int>>(
          "4,  1000,  23");
  EXPECT_EQ(std::vector<unsigned int>({4, 1000, 23}), decoded);
}

TEST(FieldTrialParamConversionsTest, DecodeBadValues) {
  auto decoded_surface = blink::IdentifiableSurface();
  EXPECT_FALSE(DecodeIdentifiabilityType("foo", &decoded_surface));
  EXPECT_FALSE(DecodeIdentifiabilityType("-100", &decoded_surface));
  EXPECT_FALSE(DecodeIdentifiabilityType("100000000000000000000000000",
                                         &decoded_surface));
}

TEST(FieldTrialParamConversionsTest, DecodeBadTypes) {
  auto decoded_type = blink::IdentifiableSurface::Type::kReservedInternal;
  EXPECT_FALSE(DecodeIdentifiabilityType("foo", &decoded_type));
  EXPECT_FALSE(DecodeIdentifiabilityType("-100", &decoded_type));
  EXPECT_FALSE(DecodeIdentifiabilityType("256", &decoded_type));
}

}  // namespace privacy_budget_internal
