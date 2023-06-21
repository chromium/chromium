// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chrome/browser/ui/views/external_protocol_dialog_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

// Tests that keyboard focus works when the dialog is shown. Regression test for
// https://crbug.com/1025343.
IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestFocus) {
  ShowUi(std::string("https://example.test"));
  gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();
#if BUILDFLAG(IS_MAC)
  // This dialog's default focused control is the Cancel button, but on Mac,
  // the cancel button cannot have initial keyboard focus. Advance focus once
  // on Mac to test whether keyboard focus advancement works there rather than
  // testing for initial focus.
  focus_manager->AdvanceFocus(false);
#endif
  const views::View* focused_view = focus_manager->GetFocusedView();
  EXPECT_TRUE(focused_view);
}
