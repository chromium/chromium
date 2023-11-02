// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_tab_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(VrTabHelper, NullWebContents) {
  EXPECT_FALSE(VrTabHelper::IsInVr(nullptr));
}

}  // namespace vr
