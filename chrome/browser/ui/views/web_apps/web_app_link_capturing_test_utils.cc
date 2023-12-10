// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"

#include "base/check_is_test.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"

namespace web_app {

IntentChipVisibilityObserver::IntentChipVisibilityObserver(
    IntentChipButton* intent_chip) {
  CHECK_IS_TEST();
  observation_.Observe(intent_chip);
}

IntentChipVisibilityObserver::~IntentChipVisibilityObserver() = default;

void IntentChipVisibilityObserver::WaitForChipToBeVisible() {
  run_loop_.Run();
}

void IntentChipVisibilityObserver::OnChipVisibilityChanged(bool is_visible) {
  if (is_visible) {
    run_loop_.Quit();
  }
}

}  // namespace web_app
