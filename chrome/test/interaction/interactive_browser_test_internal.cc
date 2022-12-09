// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test_internal.h"

#include <memory>
#include <sstream>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/native_widget_types.h"

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

void InteractiveBrowserTestPrivate::AddInstrumentedWebContents(
    std::unique_ptr<WebContentsInteractionTestUtil> instrumented_web_contents) {
  for (const auto& existing : instrumented_web_contents_) {
    CHECK_NE(instrumented_web_contents->page_identifier(),
             existing->page_identifier());
  }
  instrumented_web_contents_.emplace_back(std::move(instrumented_web_contents))
      .get();
}

std::string InteractiveBrowserTestPrivate::DeepQueryToString(
    const WebContentsInteractionTestUtil::DeepQuery& deep_query) {
  std::ostringstream oss;
  oss << "{";
  for (size_t i = 0; i < deep_query.size(); ++i) {
    if (i) {
      oss << ", ";
    }
    oss << "\"" << deep_query[i] << "\"";
  }
  oss << "}";
  return oss.str();
}

gfx::NativeWindow InteractiveBrowserTestPrivate::GetNativeWindowFromElement(
    ui::TrackedElement* el) const {
  gfx::NativeWindow window = gfx::NativeWindow();

  // For instrumented WebContents, we can get the native window directly from
  // the contents object.
  if (el->IsA<TrackedElementWebContents>()) {
    auto* const util = el->AsA<TrackedElementWebContents>()->owner();
    window = util->web_contents()->GetTopLevelNativeWindow();
  }

  // If that did not work, fall back to the base implementation.
  if (!window)
    window = InteractiveViewsTestPrivate::GetNativeWindowFromElement(el);
  return window;
}

gfx::NativeWindow InteractiveBrowserTestPrivate::GetNativeWindowFromContext(
    ui::ElementContext context) const {
  // Defer to the base implementation first, since there may be a cached value
  // that is more accurate than what can be inferred from the context.
  gfx::NativeWindow window =
      InteractiveViewsTestPrivate::GetNativeWindowFromContext(context);

  // If that didn't work, fall back to the top-level browser window for the
  // context (assuming there is one).
  if (!window) {
    if (Browser* const browser =
            InteractionTestUtilBrowser::GetBrowserFromContext(context)) {
      if (BrowserView* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser)) {
        window = browser_view->GetNativeWindow();
      }
    }
  }
  return window;
}

}  // namespace internal
