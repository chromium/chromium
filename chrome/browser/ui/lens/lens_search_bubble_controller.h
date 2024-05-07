// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace lens {

// Manages the SearchBubble instance for the associated browser.
class LensSearchBubbleController
  : public BrowserUserData<LensSearchBubbleController> {
 public:
  LensSearchBubbleController(const LensSearchBubbleController&) = delete;
  LensSearchBubbleController& operator=(const LensSearchBubbleController&)
    = delete;
  ~LensSearchBubbleController() override;

  // Shows an instance of the lens search bubble for this browser.
  void Show();
  // Closes the instance of the lens search bubble.
  void Close();

  const WebUIBubbleDialogView* bubble_view_for_testing() {
    return bubble_view_.get();
  }

 private:
  friend class BrowserUserData<LensSearchBubbleController>;

  explicit LensSearchBubbleController(Browser* browser);

  base::WeakPtr<WebUIBubbleDialogView> bubble_view_;

  BROWSER_USER_DATA_KEY_DECL();
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_BUBBLE_CONTROLLER_H_
