// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_BUBBLE_DIALOG_VIEW_H_

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;
class Profile;

namespace views {
class StyledLabel;
}  // namespace views

class PriceTrackingBubbleDialogView : public LocationBarBubbleDelegateView {
 public:
  using OnTrackPriceCallback = base::OnceCallback<void(bool)>;

  enum Type {
    // Shows if Price Tracking is disabled and user has never acted on it
    // before.
    TYPE_FIRST_USE_EXPERIENCE,
    // Shows if Price Tracking is enabled or user has been through the
    // FIRST_USE_EXPERIENCE
    // bubble.
    TYPE_NORMAL
  };

  PriceTrackingBubbleDialogView(View* anchor_view,
                                content::WebContents* web_contents,
                                Profile* profile,
                                const GURL& url,
                                ui::ImageModel image_model,
                                OnTrackPriceCallback on_track_price_callback,
                                Type type);
  ~PriceTrackingBubbleDialogView() override;

  Type GetTypeForTesting() { return type_; }
  views::StyledLabel* GetBodyLabelForTesting() { return body_label_.get(); }

 private:
  void ShowBookmarkEditor();
  void OnAccepted(OnTrackPriceCallback on_track_price_callback);
  void OnCanceled(OnTrackPriceCallback on_track_price_callback);

  const raw_ptr<Profile> profile_;
  const GURL url_;
  const Type type_;
  raw_ptr<views::StyledLabel> body_label_;

  base::WeakPtrFactory<PriceTrackingBubbleDialogView> weak_factory_{this};
};

class PriceTrackingBubbleCoordinator {
 public:
  explicit PriceTrackingBubbleCoordinator(views::View* anchor_view);
  ~PriceTrackingBubbleCoordinator();

  void Show(content::WebContents* web_contents,
            Profile* profile,
            const GURL& url,
            ui::ImageModel image_model,
            PriceTrackingBubbleDialogView::OnTrackPriceCallback callback,
            PriceTrackingBubbleDialogView::Type type);

  PriceTrackingBubbleDialogView* GetBubble() const;

 private:
  const raw_ptr<views::View> anchor_view_;
  views::ViewTracker tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_BUBBLE_DIALOG_VIEW_H_
