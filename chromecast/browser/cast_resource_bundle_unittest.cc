// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle.h"

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"

using ::testing::Return;

namespace ui {

class CastResourceBundleTest : public testing::Test {
 public:
  CastResourceBundleTest() {}
  ~CastResourceBundleTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CastResourceBundleTest);
};

TEST_F(CastResourceBundleTest, DelegateLoadLocalizedResourceBytes) {
  MockResourceBundleDelegate delegate;
  ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", &delegate, ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  unsigned char data[] = "My test data";
  scoped_refptr<base::RefCountedStaticMemory> static_memory(
      new base::RefCountedStaticMemory(data, sizeof(data)));

  int resource_id = 5;

  EXPECT_CALL(delegate,
              LoadDataResourceBytes(resource_id, ui::SCALE_FACTOR_NONE))
      .Times(1)
      .WillOnce(Return(static_memory.get()));

  scoped_refptr<base::RefCountedMemory> result =
      resource_bundle.LoadLocalizedResourceBytes(resource_id);
  EXPECT_EQ(static_memory, result);
  ResourceBundle::CleanupSharedInstance();
}

}  // namespace ui
