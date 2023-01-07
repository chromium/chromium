// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/sharesheet_mojom_traits.h"

#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace mojom {

namespace {

template <typename MojomType, typename T>
void RoundTrip(T value, MojomType mojomValue) {
  using Traits = mojo::EnumTraits<MojomType, T>;

  EXPECT_EQ(Traits::ToMojom(value), mojomValue);

  T output = T();
  EXPECT_TRUE(Traits::FromMojom(mojomValue, &output));
  EXPECT_EQ(output, value);
}

}  // namespace

// Test that every value in sharesheet::LaunchSource is
// correctly converted.
TEST(SharesheetTraitsTest, SharesheetLaunchSource) {
  RoundTrip(sharesheet::LaunchSource::kUnknown,
            SharesheetLaunchSource::kUnknown);
  RoundTrip(sharesheet::LaunchSource::kWebShare,
            SharesheetLaunchSource::kWebShare);
  RoundTrip(sharesheet::LaunchSource::kOmniboxShare,
            SharesheetLaunchSource::kOmniboxShare);
}

// Test that every value in sharesheet::SharesheetResult is correctly converted.
TEST(SharesheetTraitsTest, SharesheetResult) {
  RoundTrip(sharesheet::SharesheetResult::kSuccess, SharesheetResult::kSuccess);
  RoundTrip(sharesheet::SharesheetResult::kCancel, SharesheetResult::kCancel);
  RoundTrip(sharesheet::SharesheetResult::kErrorAlreadyOpen,
            SharesheetResult::kErrorAlreadyOpen);
  RoundTrip(sharesheet::SharesheetResult::kErrorWindowClosed,
            SharesheetResult::kErrorWindowClosed);
}

}  // namespace mojom
}  // namespace crosapi
