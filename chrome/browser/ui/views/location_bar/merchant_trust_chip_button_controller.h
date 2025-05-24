// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MERCHANT_TRUST_CHIP_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MERCHANT_TRUST_CHIP_BUTTON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/page_info/core/page_info_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"

class OmniboxChipButton;
class LocationIconView;

namespace page_info {
class MerchantTrustService;
}  // namespace page_info

// A controller for a chip-style button which opens "Merchant trust" page info
// subpage.
class MerchantTrustChipButtonController : public content::WebContentsObserver {
 public:
  constexpr static const void* const kChipAnimated = &kChipAnimated;

  MerchantTrustChipButtonController(OmniboxChipButton* chip_button,
                                    LocationIconView* location_icon_view,
                                    page_info::MerchantTrustService* service);
  MerchantTrustChipButtonController(const MerchantTrustChipButtonController&) =
      delete;
  MerchantTrustChipButtonController& operator=(
      const MerchantTrustChipButtonController&) = delete;
  ~MerchantTrustChipButtonController() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  void UpdateWebContents(content::WebContents* web_contents);

  void Show();
  void Hide();

  // Whether the chip should be visible based on the service response. Note:
  // it doesn't mean that it will be visible since other views can take
  // precedence over it.
  bool ShouldBeVisible();

 private:
  void FetchData();
  void OnMerchantTrustDataFetched(
      const GURL& url,
      std::optional<page_info::MerchantData> merchant_data);

  void StartCollapseTimer();
  void Collapse();

  void OpenPageInfoSubpage();

  void MaybeShowMerchantTrustFeaturePromo();

  raw_ptr<OmniboxChipButton> chip_button_;
  raw_ptr<LocationIconView> location_icon_view_;
  raw_ptr<page_info::MerchantTrustService> service_;
  std::optional<page_info::MerchantData> merchant_data_;
  base::OneShotTimer collapse_timer_;

  base::WeakPtrFactory<MerchantTrustChipButtonController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_MERCHANT_TRUST_CHIP_BUTTON_CONTROLLER_H_
