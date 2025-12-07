// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
#define CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_

#include <concepts>
#include <utility>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_window_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/interactive_views_test.h"

// Provides interactive test functionality for desktop browsers.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see README.md for more information).
//
// This class is not a test fixture; it is a mixin that can be added to an
// existing browser test class using `InteractiveBrowserTestMixin<T>` - or just
// use `InteractiveBrowserTest`, which *is* a test fixture (preferred; see
// below).
class InteractiveBrowserTestApi
    : virtual public views::test::InteractiveViewsTestApi,
      virtual public InteractiveBrowserWindowTestApi {
 public:
  // These methods have multiple implementations in base classes; include them
  // all in the class namespace.
  using InteractiveBrowserWindowTestApi::EnsureNotPresent;
  using InteractiveBrowserWindowTestApi::EnsurePresent;
  using InteractiveBrowserWindowTestApi::ScrollIntoView;
  using InteractiveViewsTestApi::EnsureNotPresent;
  using InteractiveViewsTestApi::EnsurePresent;
  using InteractiveViewsTestApi::ScrollIntoView;

  // Instruments the WebContents held by `web_view` as `id`. Will wait for the
  // WebView to become visible if it is not.
  //
  // If `wait_for_ready` is true (default), the step will not complete until the
  // current page in the WebContents is fully loaded. (Note that this may not
  // cover dynamic loading of data; you may need to do a WaitForStateChange() to
  // be sure dynamic content is loaded).
  [[nodiscard]] MultiStep InstrumentNonTabWebView(ui::ElementIdentifier id,
                                                  ElementSpecifier web_view,
                                                  bool wait_for_ready = true);
  [[nodiscard]] MultiStep InstrumentNonTabWebView(
      ui::ElementIdentifier id,
      AbsoluteViewSpecifier web_view,
      bool wait_for_ready = true);

  // These are required so the following overloads don't hide the base class
  // variations.
  using InteractiveViewsTestApi::DragMouseTo;
  using InteractiveViewsTestApi::MoveMouseTo;

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Move the mouse to
  // the element's center point in screen coordinates.
  //
  // If the DOM element may be scrolled outside of the current viewport,
  // consider using ScrollIntoView(web_contents, where) before this verb.
  [[nodiscard]] MultiStep MoveMouseTo(ui::ElementIdentifier web_contents,
                                      const DeepQuery& where);

  // Find the DOM element at the given path in the reference element, which
  // should be an instrumented WebContents; see Instrument*(). Perform a drag
  // from the mouse's current location to the element's center point in screen
  // coordinates, and then if `release` is true, releases the mouse button.
  //
  // If the DOM element may be scrolled outside of the current viewport,
  // consider using ScrollIntoView(web_contents, where) before this verb.
  [[nodiscard]] MultiStep DragMouseTo(ui::ElementIdentifier web_contents,
                                      const DeepQuery& where,
                                      bool release = true);
};

// Template for adding InteractiveBrowserTestApi to any test fixture which is
// derived from InProcessBrowserTest.
//
// If you don't need to derive from some existing test class, prefer to use
// InteractiveBrowserTest.
//
// Note that this test fixture attempts to set the context widget from the
// created `browser()` during `SetUpOnMainThread()`. If your derived test
// fixture does not create a browser during set up, you will need to manually
// `SetContextWidget()` before calling `RunTestSequence()`, or use
// `RunTestTestSequenceInContext()` instead.
//
// See README.md for usage.
template <typename T>
  requires std::derived_from<T, InProcessBrowserTest>
class InteractiveBrowserTestMixin : public T, public InteractiveBrowserTestApi {
 public:
  template <typename... Args>
  explicit InteractiveBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~InteractiveBrowserTestMixin() override = default;

 protected:
  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    if (Browser* browser = T::browser()) {
      SetContextWidget(
          BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
    }
  }

  void TearDownOnMainThread() override {
    private_test_impl().DoTestTearDown();
    T::TearDownOnMainThread();
  }
};

// Convenience test fixture for interactive browser tests. This is the preferred
// base class for Kombucha tests unless you specifically need something else.
//
// Note that this test fixture attempts to set the context widget from the
// created `browser()` during `SetUpOnMainThread()`. If your derived test
// fixture does not create a browser during set up, you will need to manually
// `SetContextWidget()` before calling `RunTestSequence()`, or use
// `RunTestTestSequenceInContext()` instead.
//
// See README.md for usage.
using InteractiveBrowserTest =
    InteractiveBrowserTestMixin<InProcessBrowserTest>;

#endif  // CHROME_TEST_INTERACTION_INTERACTIVE_BROWSER_TEST_H_
