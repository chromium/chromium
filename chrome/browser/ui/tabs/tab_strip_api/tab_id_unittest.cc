// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs_api {
namespace {

TEST(TabsApiTabIdTest, Comparison) {
  ASSERT_TRUE(TabId(TabId::Type::kContent, "aaa") ==
              TabId(TabId::Type::kContent, "aaa"));

  ASSERT_TRUE(TabId(TabId::Type::kContent, "aaa") !=
              TabId(TabId::Type::kCollection, "aaa"));

  ASSERT_TRUE(TabId(TabId::Type::kCollection, "aaa") !=
              TabId(TabId::Type::kCollection, "bbb"));

  ASSERT_TRUE(TabId(TabId::Type::kContent, "aaa") !=
              TabId(TabId::Type::kCollection, "bbb"));
}

}  // namespace
}  // namespace tabs_api
