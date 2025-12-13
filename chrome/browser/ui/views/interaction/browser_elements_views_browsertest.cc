// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/interaction/browser_elements_views.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/typed_identifier.h"

using BrowserElementsViewsBrowsertest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserElementsViewsBrowsertest, GetView) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNotAppearingInThisBrowserElementId);

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const elements = BrowserElementsViews::From(browser());
  EXPECT_EQ(browser_view, elements->GetView(kBrowserViewElementId));
  EXPECT_EQ(browser_view->toolbar(),
            elements->GetViewAs<ToolbarView>(ToolbarView::kToolbarElementId));
  EXPECT_EQ(nullptr, elements->GetView(kNotAppearingInThisBrowserElementId));
}

IN_PROC_BROWSER_TEST_F(BrowserElementsViewsBrowsertest, GetContext) {
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const elements = BrowserElementsViews::From(browser());
  EXPECT_EQ(browser_view->GetElementContext(), elements->GetContext());
}

IN_PROC_BROWSER_TEST_F(BrowserElementsViewsBrowsertest,
                       GetPrimaryWindowWidget) {
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const elements = BrowserElementsViews::From(browser());
  EXPECT_EQ(browser_view->GetWidget(), elements->GetPrimaryWindowWidget());
}

IN_PROC_BROWSER_TEST_F(BrowserElementsViewsBrowsertest, RetrieveView) {
  DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(ToolbarView, kToolbarViewRetrievalId);
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const elements = BrowserElementsViews::From(browser());
  elements->AddRetrievalCallback(kToolbarViewRetrievalId,
                                 base::BindLambdaForTesting([browser_view]() {
                                   return browser_view->toolbar();
                                 }));
  EXPECT_EQ(browser_view->toolbar(),
            elements->RetrieveView(kToolbarViewRetrievalId));
}
