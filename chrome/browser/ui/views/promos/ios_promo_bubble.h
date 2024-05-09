// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_

// #include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_constants.h"

namespace views {
class PageActionIconView;
class View;
}  // namespace views

class Browser;

enum class IOSPromoType;

// A view for the iOS bubble promo which displays a QR code.
class IOSPromoBubble {
 private:
  // SetUpBubble sets up the promo constants (such as strings) depending
  // on the given promo type and returns the current IOSPromoBubble's config.
  static IOSPromoConstants::IOSPromoTypeConfigs SetUpBubble(
      IOSPromoType promo_type);

 public:
  IOSPromoBubble(const IOSPromoBubble&) = delete;
  IOSPromoBubble& operator=(const IOSPromoBubble&) = delete;

  // ShowBubble creates the view and shows the bubble to the user, attached
  // to the feature icon.
  static void ShowPromoBubble(views::View* anchor_view,
                              PageActionIconView* highlighted_button,
                              Browser* browser,
                              IOSPromoType promo_type);

  // Hide closes the bubble.
  static void Hide();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_BUBBLE_H_
