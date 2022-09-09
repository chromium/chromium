// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_

#include "build/build_config.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/in_process_browser_test.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

// A dialog-specific subclass of TestBrowserUi, which will verify that a test
// showed a single dialog.
class TestBrowserDialog : public TestBrowserUi {
 public:
  TestBrowserDialog(const TestBrowserDialog&) = delete;
  TestBrowserDialog& operator=(const TestBrowserDialog&) = delete;

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

  // Verify UI.
  // When pixel verifcation is enabled(--browser-ui-tests-verify-pixels),
  // this function will also verify pixels using Skia Gold. Call set_baseline()
  // and SetPixelMatchAlgorithm() to adjust parameters used for verification.
  void ShowAndVerifyUi();

  // Only useful when pixel verification is enabled.
  // Set pixel test baseline so previous gold images become invalid.
  // Call this method before ShowAndVerifyUi().
  // For example, a cl changes a dialog's text, and all previously approved
  // gold images become invalid. Then in the same cl you should set a new
  // baseline. Or else the previous gold image are still valid (which they
  // should not be because they have wrong text).
  // Consider using the cl number as baseline.
  void set_baseline(const std::string& baseline) { baseline_ = baseline; }

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

  // The baseline to use for the next pixel verification.
  std::string baseline_;

  // If set to true, the dialog bounds will be verified to fit inside the
  // display's work area.
  // This should always be true, but some dialogs don't yet size themselves
  // properly. https://crbug.com/893292.
  bool should_verify_dialog_bounds_ = true;
};

template <class Base>
using SupportsTestDialog = SupportsTestUi<Base, TestBrowserDialog>;

using DialogBrowserTest = SupportsTestDialog<InProcessBrowserTest>;

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_DIALOG_H_
