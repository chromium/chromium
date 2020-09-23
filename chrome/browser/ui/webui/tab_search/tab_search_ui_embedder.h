// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_EMBEDDER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_EMBEDDER_H_

// Interface to be implemented by the embedder. Provides native UI
// functionality such as showing and closing a bubble view.
class TabSearchUIEmbedder {
 public:
  TabSearchUIEmbedder() = default;
  virtual ~TabSearchUIEmbedder() = default;

  virtual void ShowBubble() = 0;
  virtual void CloseBubble() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_SEARCH_TAB_SEARCH_UI_EMBEDDER_H_
