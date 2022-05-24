// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_DIALOG_VIEW_H_

#include <vector>

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

class ToolbarActionViewController;

void ShowExtensionsRequestAccessDialogView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    std::vector<ToolbarActionViewController*> extensions);

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_DIALOG_VIEW_H_
