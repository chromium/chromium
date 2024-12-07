// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_animated_gradient_view.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/test_animation_delegate.h"

namespace autofill_ai {
namespace {

using AutofillAiAnimatedGradientViewTest = ChromeViewsTestBase;

TEST_F(AutofillAiAnimatedGradientViewTest, StopsAnimatingWhenDestroyed) {
  gfx::TestAnimationDelegate animation_delegate;
  {
    AutofillAiAnimatedGradientView v;
    EXPECT_TRUE(v.IsAnimatingForTest());
    v.SetAnimationDelegateForTest(&animation_delegate);
    EXPECT_FALSE(animation_delegate.finished());
  }
  EXPECT_TRUE(animation_delegate.finished());
}

}  // namespace
}  // namespace autofill_ai
