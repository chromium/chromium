// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADD_NEW_ADDRESS_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADD_NEW_ADDRESS_BUBBLE_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/add_new_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/address_bubble_base_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class View;
}  // namespace views

namespace autofill {
// This is the bubble view prompting the user to save a new address (used for
// the flow when there is no addresses saved, and the manual fallback context
// menu item for addresses gets triggered).
class AddNewAddressBubbleView : public AddressBubbleBaseView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopViewId);

  AddNewAddressBubbleView(
      std::unique_ptr<AddNewAddressBubbleController> controller,
      views::View* anchor_view,
      content::WebContents* web_contents);
  AddNewAddressBubbleView(const AddNewAddressBubbleView&) = delete;
  AddNewAddressBubbleView& operator=(const AddNewAddressBubbleView&) = delete;
  ~AddNewAddressBubbleView() override;

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // AutofillBubbleBase:
  void Hide() override;

  // View:
  void AddedToWidget() override;

 private:
  std::unique_ptr<AddNewAddressBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADD_NEW_ADDRESS_BUBBLE_VIEW_H_
