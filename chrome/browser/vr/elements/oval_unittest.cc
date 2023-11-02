// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/oval.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(Oval, CornerRadii) {
  Oval oval;
  oval.SetSize(10.0f, 1.0f);
  EXPECT_FLOAT_EQ(0.5f, oval.corner_radius());
}

}  // namespace vr
