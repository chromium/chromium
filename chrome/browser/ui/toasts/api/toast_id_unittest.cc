// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/api/toast_id.h"

#include "base/containers/enum_set.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using ToastIdEnumSet =
    base::EnumSet<ToastId, ToastId::kMinValue, ToastId::kMaxValue>;
}

using ToastIdUnitTest = testing::Test;

// Each Toast ID should have a corresponding string name for metrics purposes.
TEST_F(ToastIdUnitTest, EachToastIdHaveStringName) {
  for (ToastId id : ToastIdEnumSet::All()) {
    EXPECT_FALSE(GetToastName(id).empty());
  }
}
