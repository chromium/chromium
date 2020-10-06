// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

namespace content {
class WebContents;
}

namespace views {
class View;
}

namespace autofill {

// This is the bubble views that is part of the flow for when the user submits a
// form with an address profile that Autofill has not previously saved.
class SaveAddressProfileView : public LocationBarBubbleDelegateView {
 public:
  SaveAddressProfileView(views::View* anchor_view,
                         content::WebContents* web_contents);

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override;
  base::string16 GetWindowTitle() const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ADDRESS_PROFILES_SAVE_ADDRESS_PROFILE_VIEW_H_
