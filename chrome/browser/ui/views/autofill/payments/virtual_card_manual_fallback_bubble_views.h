// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_VIEWS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace views {
class MdTextButton;
}

namespace autofill {

// This class implements the desktop bubble that displays the information of the
// virtual card that was sent to Chrome from Payments.
class VirtualCardManualFallbackBubbleViews : public AutofillLocationBarBubble {
  METADATA_HEADER(VirtualCardManualFallbackBubbleViews,
                  AutofillLocationBarBubble)
 public:
  // The bubble will be anchored to the |anchor_view|.
  VirtualCardManualFallbackBubbleViews(
      views::View* anchor_view,
      content::WebContents* web_contents,
      VirtualCardManualFallbackBubbleController* controller);
  ~VirtualCardManualFallbackBubbleViews() override;
  VirtualCardManualFallbackBubbleViews(
      const VirtualCardManualFallbackBubbleViews&) = delete;
  VirtualCardManualFallbackBubbleViews& operator=(
      const VirtualCardManualFallbackBubbleViews&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      VirtualCardManualFallbackBubbleViewsInteractiveUiTest,
      TooltipAndAccessibleName);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void Init() override;
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Creates a button for the |field|. If the button is pressed, the text of it
  // will be copied to the clipboard.
  std::unique_ptr<views::MdTextButton> CreateRowItemButtonForField(
      VirtualCardManualFallbackBubbleField field);

  // Adds a container view to show the card art image, card name, card
  // number last four and "Virtual card" indicator label.
  void AddCardDescriptionView(views::View* parent);

  // Adds a list of pairs of label and button to show the virtual card details
  // and to allow user to copy the details to clipboard.
  void AddCardDetailButtons(views::View* parent);

  // Invoked when a button with card information is clicked.
  void OnFieldClicked(VirtualCardManualFallbackBubbleField field);

  // Update the tooltips and the accessible names of the buttons.
  void UpdateButtonTooltipsAndAccessibleNames();

  // Handles user click on learn more link.
  void LearnMoreLinkClicked();

  raw_ptr<VirtualCardManualFallbackBubbleController> controller_;

  // The map keeping the references to each button with card information text in
  // the bubble.
  std::map<VirtualCardManualFallbackBubbleField,
           raw_ptr<views::MdTextButton, CtnExperimental>>
      fields_to_buttons_map_;

  base::WeakPtrFactory<VirtualCardManualFallbackBubbleViews> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_VIEWS_H_
