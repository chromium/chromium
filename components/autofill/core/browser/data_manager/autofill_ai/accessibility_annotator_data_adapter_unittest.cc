// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/accessibility_annotator_data_adapter.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::IsEmpty;

class AccessibilityAnnotatorDataAdapterTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AccessibilityAnnotatorDataAdapterTest, GetEntityInstances_IsEmpty) {
  AccessibilityAnnotatorDataAdapter adapter;
  EXPECT_THAT(adapter.GetEntityInstances(), IsEmpty());
}

}  // namespace
}  // namespace autofill
