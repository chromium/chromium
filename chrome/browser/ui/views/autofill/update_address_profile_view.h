// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_UPDATE_ADDRESS_PROFILE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_UPDATE_ADDRESS_PROFILE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace autofill {
class SaveUpdateAddressProfileBubbleController;

// Shown after a user submits a form with an address profile that's slightly
// different from an address profile previously saved.
class UpdateAddressProfileView : public AutofillBubbleBase,
                                 public LocationBarBubbleDelegateView {
 public:
  UpdateAddressProfileView(
      views::View* anchor_view,
      content::WebContents* web_contents,
      SaveUpdateAddressProfileBubbleController* controller);

  UpdateAddressProfileView(const UpdateAddressProfileView&) = delete;
  UpdateAddressProfileView& operator=(const UpdateAddressProfileView&) = delete;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

 private:
  raw_ptr<SaveUpdateAddressProfileBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_UPDATE_ADDRESS_PROFILE_VIEW_H_
