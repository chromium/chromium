// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"

using ::testing::Return;

namespace ui {

class CastResourceBundleTest : public testing::Test {
 public:
  CastResourceBundleTest() {}

  CastResourceBundleTest(const CastResourceBundleTest&) = delete;
  CastResourceBundleTest& operator=(const CastResourceBundleTest&) = delete;

  ~CastResourceBundleTest() override {}
};

TEST_F(CastResourceBundleTest, DelegateLoadLocalizedResourceBytes) {
  MockResourceBundleDelegate delegate;

  // A ResourceBundle that uses the test's mock delegate.
  ui::ResourceBundle resource_bundle_with_mock_delegate{&delegate};

  // Swap in the test ResourceBundle for the lifetime of the test.
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_bundle_swapper{
      &resource_bundle_with_mock_delegate};

  const unsigned char data[] = "My test data";
  scoped_refptr<base::RefCountedStaticMemory> static_memory(
      new base::RefCountedStaticMemory(data));

  int resource_id = 5;

  EXPECT_CALL(delegate,
              LoadDataResourceBytes(resource_id, ui::kScaleFactorNone))
      .Times(1)
      .WillOnce(Return(static_memory.get()));

  scoped_refptr<base::RefCountedMemory> result =
      ResourceBundle::GetSharedInstance().LoadLocalizedResourceBytes(
          resource_id);
  EXPECT_EQ(static_memory, result);
}

}  // namespace ui
