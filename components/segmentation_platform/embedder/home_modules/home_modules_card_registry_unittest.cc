// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

class HomeModulesCardRegistryTest : public testing::Test {
 public:
  HomeModulesCardRegistryTest() = default;
  ~HomeModulesCardRegistryTest() override = default;

  void SetUp() override { Test::SetUp(); }

  void TearDown() override { Test::TearDown(); }

 protected:
};

TEST_F(HomeModulesCardRegistryTest, Test) {}

}  // namespace segmentation_platform::home_modules
