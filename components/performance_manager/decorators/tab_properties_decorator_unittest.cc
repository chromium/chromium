// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/tab_properties_decorator.h"

#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class TabPropertiesDecoratorTest : public PerformanceManagerTestHarness {
 public:
  TabPropertiesDecoratorTest() = default;
  ~TabPropertiesDecoratorTest() override = default;
  TabPropertiesDecoratorTest(const TabPropertiesDecoratorTest& other) = delete;
  TabPropertiesDecoratorTest& operator=(const TabPropertiesDecoratorTest&) =
      delete;

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());
  }

  void TearDown() override {
    DeleteContents();
    PerformanceManagerTestHarness::TearDown();
  }
};

TEST_F(TabPropertiesDecoratorTest, SetIsTab) {
  testing::EndToEndBooleanPropertyTest(
      web_contents(), &TabPropertiesDecorator::Data::GetOrCreateForTesting,
      &TabPropertiesDecorator::Data::IsInTabStrip,
      &TabPropertiesDecorator::SetIsTab);
}

}  // namespace performance_manager