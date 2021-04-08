// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_BROWSER_UI_H_
#define CHROME_BROWSER_UI_TEST_TEST_BROWSER_UI_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace views {
class Widget;
class View;
}  // namespace views

namespace ui {
namespace test {
class SkiaGoldMatchingAlgorithm;
}  // namespace test
}  // namespace ui

// TestBrowserUi provides a way to register an InProcessBrowserTest testing
// harness with a framework that invokes Chrome browser UI in a consistent way.
// It optionally provides a way to invoke UI "interactively". This allows
// screenshots to be generated easily, with the same test data, to assist with
// UI review. It also provides a UI registry so pieces of UI can be
// systematically checked for subtle changes and regressions.
//
// To use TestBrowserUi, a test harness should inherit from UiBrowserTest rather
// than InProcessBrowserTest, then provide some overrides:
//
// class FooUiTest : public UiBrowserTest {
//  public:
//   ..
//   // UiBrowserTest:
//   void ShowUi(const std::string& name) override {
//     /* Show Ui attached to browser() and leave it open. */
//   }
//
//   bool VerifyUi() override {
//     /* Return true if the UI was successfully shown. */
//   }
//
//   void WaitForUserDismissal() override {
//     /* Block until the user closes the UI. */
//   }
//   ..
// };
//
// Further overrides are available for tests which need to do work before
// showing any UI or when closing in non-interactive mode.  For tests whose UI
// is a dialog, there's also the TestBrowserDialog class, which provides all but
// ShowUi() already; see test_browser_dialog.h.
//
// The test may then define any number of cases for individual pieces of UI:
//
// IN_PROC_BROWSER_TEST_F(FooUiTest, InvokeUi_name) {
//   // Perform optional setup here; then:
//   ShowAndVerifyUi();
// }
//
// The string after "InvokeUi_" (here, "name") is the argument given to
// ShowUi(). In a regular test suite run, ShowAndVerifyUi() shows the UI and
// immediately closes it (after ensuring it was actually created).
//
// To get a list of all available UI, run the "BrowserUiTest.Invoke" test case
// without other arguments, i.e.:
//
//   browser_tests --gtest_filter=BrowserUiTest.Invoke
//
// UI listed can be shown interactively using the --ui argument. E.g.
//
//   browser_tests --gtest_filter=BrowserUiTest.Invoke
//       --test-launcher-interactive --ui=FooUiTest.InvokeUi_name
class TestBrowserUi {
 protected:
  TestBrowserUi();
  virtual ~TestBrowserUi();

  // Called by ShowAndVerifyUi() before ShowUi(), to provide a place to do any
  // setup needed in order to successfully verify the UI post-show.
  virtual void PreShow() {}

  // Should be implemented in individual tests to show UI with the given |name|
  // (which will be supplied by the test case).
  virtual void ShowUi(const std::string& name) = 0;

  // Called by ShowAndVerifyUi() after ShowUi().  Returns whether the UI was
  // successfully shown.
  virtual bool VerifyUi() = 0;

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // Can be called by VerifyUi() to ensure pixel correctness.
  bool VerifyPixelUi(views::Widget* widget,
                     const std::string& screenshot_prefix,
                     const std::string& screenshot_name);

  // Can be called by VerifyUi() to ensure pixel correctness.
  bool VerifyPixelUi(views::View* view,
                     const std::string& screenshot_prefix,
                     const std::string& screenshot_name);

  // Own |algorithm|.
  void SetPixelMatchAlgorithm(
      std::unique_ptr<ui::test::SkiaGoldMatchingAlgorithm> algorithm);
  ui::test::SkiaGoldMatchingAlgorithm* GetPixelMatchAlgorithm() {
    return algorithm_.get();
  }
#endif

  // Called by ShowAndVerifyUi() after VerifyUi(), in the case where the test is
  // interactive.  This should block until the UI has been dismissed.
  virtual void WaitForUserDismissal() = 0;

  // Called by ShowAndVerifyUi() after VerifyUi(), in the case where the test is
  // non-interactive.  This should do anything necessary to close the UI before
  // browser shutdown.
  virtual void DismissUi() {}

  // Shows the UI whose name corresponds to the current test case, and verifies
  // it was successfully shown.  Most test cases can simply invoke this directly
  // with no other code.
  void ShowAndVerifyUi();

 private:
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  std::unique_ptr<ui::test::SkiaGoldMatchingAlgorithm> algorithm_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestBrowserUi);
};

// Helper to mix in a TestBrowserUi to an existing test harness. |Base| must be
// a descendant of InProcessBrowserTest.
template <class Base, class TestUi>
class SupportsTestUi : public Base, public TestUi {
 protected:
  template <class... Args>
  explicit SupportsTestUi(Args&&... args) : Base(std::forward<Args>(args)...) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SupportsTestUi);
};

using UiBrowserTest = SupportsTestUi<InProcessBrowserTest, TestBrowserUi>;

#endif  // CHROME_BROWSER_UI_TEST_TEST_BROWSER_UI_H_
