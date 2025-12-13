// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class BubbleDialogDelegate;
class Button;
class View;
}  // namespace views

namespace IOSPromoConstants {
struct IOSPromoTypeConfigs;
}  // namespace IOSPromoConstants

class Profile;

namespace desktop_to_mobile_promos {
enum class BubbleType;
enum class PromoType;
}  // namespace desktop_to_mobile_promos
class IOSPromoBubbleView;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kIOSPromoBubbleElementId);

// A view for the bubble promo that encourages feature usage on iOS.
class IOSPromoBubble {
 public:
  class IOSPromoBubbleDelegate;

  // Specifies how the bubble should be anchored.
  struct Anchor {
    raw_ptr<views::View> view;
    views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT;
  };

  IOSPromoBubble(const IOSPromoBubble&) = delete;
  IOSPromoBubble& operator=(const IOSPromoBubble&) = delete;

  // Creates and shows the promo bubble.
  //
  // Parameters:
  //   anchor_view: The view to which the bubble will be anchored.
  //   highlighted_button: The button to highlight when the bubble is shown. May
  //   be null if no button should be highlighted. profile: The user's profile.
  //   promo_type: The feature being highlighted in the promo.
  //   bubble_type: The type of bubble to show (e.g., QR code or reminder).
  static void ShowPromoBubble(Anchor anchor,
                              views::Button* highlighted_button,
                              Profile* profile,
                              desktop_to_mobile_promos::PromoType promo_type,
                              desktop_to_mobile_promos::BubbleType bubble_type);

  // Hide closes the bubble.
  static void Hide();

  // Returns true if the bubble is currently being shown and is of type
  // `promo_type`.
  static bool IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType promo_type);

 private:
  friend class ::IOSPromoBubbleView;

  // Returns the config for the given `promo_type` and `bubble_type`.
  static IOSPromoConstants::IOSPromoTypeConfigs SetUpBubble(
      desktop_to_mobile_promos::PromoType promo_type,
      desktop_to_mobile_promos::BubbleType bubble_type);

  // Creates the content view for the promo bubble, which includes the body and
  // buttons. Depedning on the BubbleLayout, the content view takes up either
  // the entire bubble, or is added as a footer to the bubble.
  static std::unique_ptr<views::View> CreateContentView(
      IOSPromoBubble::IOSPromoBubbleDelegate* bubble_delegate,
      const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config,
      bool with_title,
      desktop_to_mobile_promos::BubbleType bubble_type);

  // Creates the body of the promo bubble, which includes the QR code or
  // icon, and the description.
  static std::unique_ptr<views::View> CreateImageAndBodyTextView(
      const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config,
      desktop_to_mobile_promos::BubbleType bubble_type);

  // Creates the buttons view for the promo bubble.
  static std::unique_ptr<views::View> CreateButtonsView(
      IOSPromoBubble::IOSPromoBubbleDelegate* bubble_delegate,
      const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config,
      desktop_to_mobile_promos::BubbleType bubble_type);

  static views::BubbleDialogDelegate* ios_promo_delegate_;

  static desktop_to_mobile_promos::PromoType current_promo_type_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_
