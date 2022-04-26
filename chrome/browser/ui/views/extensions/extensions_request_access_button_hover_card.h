// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_HOVER_CARD_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_HOVER_CARD_H_

#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

class ToolbarActionViewController;

class ExtensionsRequestAccessButtonHoverCard {
 public:
  // Creates and shows the request access button bubble.
  static void ShowBubble(content::WebContents* web_contents,
                         views::View* anchor_view,
                         std::vector<ToolbarActionViewController*> actions);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_REQUEST_ACCESS_BUTTON_HOVER_CARD_H_
