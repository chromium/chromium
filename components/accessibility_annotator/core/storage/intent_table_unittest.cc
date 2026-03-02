// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/intent_table.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

TEST(IntentTableTest, InitFalseIfNullDatabase) {
  IntentTable intent_table;
  EXPECT_FALSE(intent_table.Init(nullptr));
}

}  // namespace accessibility_annotator
