// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bidi_mapper/host/bidi_mapper_host.h"

#include "base/test/scoped_feature_list.h"
#include "components/bidi_mapper/host/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bidi_mapper {

TEST(BidiMapperHostTest, DisabledByDefault) {
  EXPECT_FALSE(BidiMapperHost::IsEnabled());
}

TEST(BidiMapperHostTest, Enabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kChromeDriverExtensionApi);
  EXPECT_TRUE(BidiMapperHost::IsEnabled());
}

TEST(BidiMapperHostTest, Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kChromeDriverExtensionApi);
  EXPECT_FALSE(BidiMapperHost::IsEnabled());
}

}  // namespace bidi_mapper
