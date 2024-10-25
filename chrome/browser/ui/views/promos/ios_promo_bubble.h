// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_

#include <memory>

namespace views {
class BubbleDialogDelegate;
class View;
}  // namespace views

namespace IOSPromoConstants {
struct IOSPromoTypeConfigs;
}  // namespace IOSPromoConstants

class PageActionIconView;
class Profile;

enum class IOSPromoType;

// A view for the iOS bubble promo which displays a QR code.
class IOSPromoBubble {
 private:
  // SetUpBubble sets up the promo constants (such as strings) depending
  // on the given promo type and returns the current IOSPromoBubble's config.
  static IOSPromoConstants::IOSPromoTypeConfigs SetUpBubble(
      IOSPromoType promo_type);

  static views::BubbleDialogDelegate* ios_promo_delegate_;
  static IOSPromoType current_promo_type_;

  class IOSPromoBubbleDelegate;

  static std::unique_ptr<views::View> CreateFooter(
      IOSPromoBubble::IOSPromoBubbleDelegate* bubble_delegate,
      const IOSPromoConstants::IOSPromoTypeConfigs& ios_promo_config);

 public:
  IOSPromoBubble(const IOSPromoBubble&) = delete;
  IOSPromoBubble& operator=(const IOSPromoBubble&) = delete;

  // ShowBubble creates the view and shows the bubble to the user, attached
  // to the feature icon.
  static void ShowPromoBubble(views::View* anchor_view,
                              PageActionIconView* highlighted_button,
                              Profile* profile,
                              IOSPromoType promo_type);

  // Hide closes the bubble.
  static void Hide();

  // Returns true if the bubble is currently being shown and is of type
  // `promo_type`.
  static bool IsPromoTypeVisible(IOSPromoType promo_type);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_
