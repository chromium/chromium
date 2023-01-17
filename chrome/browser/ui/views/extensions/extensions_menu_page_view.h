// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_PAGE_VIEW_H_

#include "ui/views/view.h"

namespace content {
class WebContents;
}  // namespace content

// An interface for pages in the extensions menu.
class ExtensionsMenuPageView : public views::View {
 public:
  // TODO(crbug.com/1390952): Move shared page view construction from "main
  // page" and "site permissions page".

  // Updates the menu page for `web_contents`.
  virtual void Update(content::WebContents* web_contents) = 0;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuPageView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_PAGE_VIEW_H_
