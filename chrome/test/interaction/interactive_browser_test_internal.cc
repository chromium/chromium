// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test_internal.h"

#include <memory>

#include "chrome/test/interaction/interaction_test_util_browser.h"

namespace internal {

InteractiveBrowserTestPrivate::InteractiveBrowserTestPrivate(
    std::unique_ptr<InteractionTestUtilBrowser> test_util)
    : InteractiveViewsTestPrivate(std::move(test_util)) {}

InteractiveBrowserTestPrivate::~InteractiveBrowserTestPrivate() = default;

void InteractiveBrowserTestPrivate::DoTestTearDown() {
  // Release any remaining instrumented WebContents.
  instrumented_web_contents_.clear();

  InteractiveViewsTestPrivate::DoTestTearDown();
}

}  // namespace internal
