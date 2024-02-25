// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_PASSWORD_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_PASSWORD_BUBBLE_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"

namespace views {
class PageActionIconView;
class View;
}  // namespace views

class Browser;

// A view for the iOS bubble promo which support 2 variants. One variant
// displays a QR code and the other a button which redirects the user to a
// download page.
class IOSPromoPasswordBubble {
 public:
  // QR code view identifier.
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kQRCodeView);

  IOSPromoPasswordBubble(const IOSPromoPasswordBubble&) = delete;
  IOSPromoPasswordBubble& operator=(const IOSPromoPasswordBubble&) = delete;

  // ShowBubble creates the view and shows the bubble to the user, attached
  // to the passwords key icon.
  static void ShowBubble(views::View* anchor_view,
                         PageActionIconView* highlighted_button,
                         Browser* browser);

  // Hide closes the bubble.
  static void Hide();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_PASSWORD_BUBBLE_H_
