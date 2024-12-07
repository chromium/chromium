// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MERCHANT_TRUST_CHIP_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MERCHANT_TRUST_CHIP_BUTTON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"

class OmniboxChipButton;
class LocationIconView;

// A controller for a chip-style button which opens "Merchant trust" page info
// subpage.
class MerchantTrustChipButtonController {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementIdForTesting);

  MerchantTrustChipButtonController(OmniboxChipButton* chip_button,
                                    LocationIconView* location_icon_view);
  MerchantTrustChipButtonController(const MerchantTrustChipButtonController&) =
      delete;
  MerchantTrustChipButtonController& operator=(
      const MerchantTrustChipButtonController&) = delete;
  virtual ~MerchantTrustChipButtonController();

  void UpdateWebContents(content::WebContents* web_contents);

  void Show();
  void Hide();
  void OpenPageInfoSubpage();

 private:
  raw_ptr<OmniboxChipButton> chip_button_;
  raw_ptr<LocationIconView> location_icon_view_;
  base::WeakPtr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MERCHANT_TRUST_CHIP_BUTTON_CONTROLLER_H_
