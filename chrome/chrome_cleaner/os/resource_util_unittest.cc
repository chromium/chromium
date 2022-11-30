// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/resource_util.h"

#include <windows.h>

#include <stdint.h>

#include "chrome/chrome_cleaner/test/resources/grit/test_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

// This resource id must not be used.
const uint32_t kUnusedResourceId = 42;

}  // namespace

TEST(ResourceUtilTest, LoadResourceWithName) {
  base::StringPiece content;
  // Invalid resource type.
  EXPECT_FALSE(LoadResourceOfKind(IDS_TEST_TEXT, L"ICON", &content));
  // Invalid resource id.
  ASSERT_FALSE(::FindResource(::GetModuleHandle(nullptr),
                              MAKEINTRESOURCE(kUnusedResourceId), L"BINDATA"));
  EXPECT_FALSE(LoadResourceOfKind(kUnusedResourceId, L"BINDATA", &content));
  // Valid resource.
  EXPECT_TRUE(LoadResourceOfKind(IDS_TEST_TEXT, L"TEXT", &content));
  EXPECT_GT(content.size(), 0U);
}

}  // namespace chrome_cleaner
