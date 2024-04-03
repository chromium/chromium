// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_SEARCH_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

namespace lens {

// Manages the SearchBubble instance for the associated browser.
class SearchBubbleController : public BrowserUserData<SearchBubbleController> {
 public:
  SearchBubbleController(const SearchBubbleController&) = delete;
  SearchBubbleController& operator=(const SearchBubbleController&) = delete;
  ~SearchBubbleController() override;

  // Shows an instance of the lens search bubble for this browser.
  void Show();

 private:
  friend class BrowserUserData<SearchBubbleController>;

  explicit SearchBubbleController(Browser* browser);

  std::unique_ptr<WebUIBubbleManager> webui_bubble_manager_;

  BROWSER_USER_DATA_KEY_DECL();
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_VIEWS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
