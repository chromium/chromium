// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_ADDRESS_PROFILE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_ADDRESS_PROFILE_VIEW_H_

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace autofill {
class SaveAddressProfileBubbleController;

// This is the bubble views that is part of the flow for when the user submits a
// form with an address profile that Autofill has not previously saved.
class SaveAddressProfileView : public AutofillBubbleBase,
                               public LocationBarBubbleDelegateView {
 public:
  SaveAddressProfileView(views::View* anchor_view,
                         content::WebContents* web_contents,
                         SaveAddressProfileBubbleController* controller);

  SaveAddressProfileView(const SaveAddressProfileView&) = delete;
  SaveAddressProfileView& operator=(const SaveAddressProfileView&) = delete;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // View:
  void AddedToWidget() override;

 private:
  SaveAddressProfileBubbleController* controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_ADDRESS_PROFILE_VIEW_H_
