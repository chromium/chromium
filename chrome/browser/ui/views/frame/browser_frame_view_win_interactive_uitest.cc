// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

class BrowserFrameViewWinInteractiveUiTest : public InteractiveBrowserTest {
 public:
  BrowserFrameViewWinInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kMenuSimplification);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserFrameViewWinInteractiveUiTest,
                       TriggerSystemMenuWithKeyboard) {
  RunTestSequence(SendKeyPress(kBrowserViewElementId,
                               ui::KeyboardCode::VKEY_SPACE, ui::EF_ALT_DOWN),
                  WaitForShow(kSystemMenuRestoreItemElementId));
}

}  // namespace
