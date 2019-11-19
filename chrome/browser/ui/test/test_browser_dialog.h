// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/in_process_browser_test.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

class BrowserSkiaGoldPixelDiff;

// A dialog-specific subclass of TestBrowserUi, which will verify that a test
// showed a single dialog.
class TestBrowserDialog : public TestBrowserUi {
 protected:
  TestBrowserDialog();
  ~TestBrowserDialog() override;

  void set_should_verify_dialog_bounds(bool value) {
    should_verify_dialog_bounds_ = value;
  }

  // TestBrowserUi:
  void PreShow() override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;
  void DismissUi() override;

  // Whether to close asynchronously using Widget::Close(). This covers
  // codepaths relying on DialogDelegate::Close(), which isn't invoked by
  // Widget::CloseNow(). Dialogs should support both, since the OS can initiate
  // the destruction of dialogs, e.g., during logoff which bypass
  // Widget::CanClose() and DialogDelegate::Close().
  virtual bool AlwaysCloseAsynchronously();

  // Get the name of a non-dialog window that should be included in testing.
  // VerifyUi() only considers dialog windows and windows with a matching name.
  virtual std::string GetNonDialogName();

 private:
#if defined(TOOLKIT_VIEWS)
  // Stores the current widgets in |widgets_|.
  void UpdateWidgets();

  // The widgets present before/after showing UI.
  views::Widget::Widgets widgets_;
#endif  // defined(TOOLKIT_VIEWS)

  // If set to true, the dialog bounds will be verified to fit inside the
  // display's work area.
  // This should always be true, but some dialogs don't yet size themselves
  // properly. https://crbug.com/893292.
  bool should_verify_dialog_bounds_ = true;
  // If this variable is set, VerifyUi will verify pixel correctness for
  // the dialog.
  std::unique_ptr<BrowserSkiaGoldPixelDiff> pixel_diff_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserDialog);
};

template <class Base>
using SupportsTestDialog = SupportsTestUi<Base, TestBrowserDialog>;

using DialogBrowserTest = SupportsTestDialog<InProcessBrowserTest>;

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_
