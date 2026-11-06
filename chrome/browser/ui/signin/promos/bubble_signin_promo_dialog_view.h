// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_DIALOG_VIEW_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/signin/public/base/signin_metrics.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BubbleSignInPromoDelegate;

namespace content {
class WebContents;
}  // namespace content

// Base dialog bubble view for standalone location bar, toolbar, or avatar pill
// sign-in promos that wrap a single `BubbleSignInPromoView` child and an
// optional Lottie header illustration.
class BubbleSignInPromoDialogView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(BubbleSignInPromoDialogView, LocationBarBubbleDelegateView)

 public:
  // `delegate` handles the sign-in button and must not be null.
  BubbleSignInPromoDialogView(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      signin_metrics::AccessPoint access_point,
      std::unique_ptr<BubbleSignInPromoDelegate> delegate,
      int signin_title_resource_id,
      std::optional<int> verify_title_resource_id = std::nullopt,
      std::optional<int> lottie_resource_id = std::nullopt);
  BubbleSignInPromoDialogView(const BubbleSignInPromoDialogView&) = delete;
  BubbleSignInPromoDialogView& operator=(const BubbleSignInPromoDialogView&) =
      delete;
  ~BubbleSignInPromoDialogView() override;

  // views::View:
  void AddedToWidget() override;

 private:
  const std::optional<int> lottie_resource_id_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_PROMOS_BUBBLE_SIGNIN_PROMO_DIALOG_VIEW_H_
