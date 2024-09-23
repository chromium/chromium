// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_ADDRESS_PROFILE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_ADDRESS_PROFILE_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/address_bubble_base_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class ImageButton;
class ImageView;
class View;
}  // namespace views

namespace autofill {
// This is the bubble views that is part of the flow for when the user submits a
// form with an address profile that Autofill has not previously saved.
class SaveAddressProfileView : public AddressBubbleBaseView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kEditButtonViewId);

  SaveAddressProfileView(
      std::unique_ptr<SaveAddressBubbleController> controller,
      views::View* anchor_view,
      content::WebContents* web_contents);

  SaveAddressProfileView(const SaveAddressProfileView&) = delete;
  SaveAddressProfileView& operator=(const SaveAddressProfileView&) = delete;
  ~SaveAddressProfileView() override;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // View:
  void AddedToWidget() override;

 private:
  // Sets the proper margins for icons (and other views) in the UI to make sure
  // all icons are vertically centered with corresponding text.
  void AlignIcons();

  std::unique_ptr<SaveAddressBubbleController> controller_;

  // The following are used for UI elements alignment upon changes in theme.
  raw_ptr<views::View> address_components_view_;
  std::vector<raw_ptr<views::ImageView, VectorExperimental>>
      address_section_icons_;
  raw_ptr<views::ImageButton> edit_button_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_ADDRESS_PROFILE_VIEW_H_
