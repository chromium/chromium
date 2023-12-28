// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_BUBBLE_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_BUBBLE_DIALOG_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;
class Profile;

namespace views {
class StyledLabel;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kPriceTrackingBubbleDialogId);

class PriceTrackingBubbleDialogView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(PriceTrackingBubbleDialogView, LocationBarBubbleDelegateView)

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

class PriceTrackingBubbleCoordinator : public views::WidgetObserver {
 public:
  explicit PriceTrackingBubbleCoordinator(views::View* anchor_view);
  ~PriceTrackingBubbleCoordinator() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void Show(content::WebContents* web_contents,
            Profile* profile,
            const GURL& url,
            ui::ImageModel image_model,
            PriceTrackingBubbleDialogView::OnTrackPriceCallback callback,
            base::OnceClosure on_dialog_closing_callback,
            PriceTrackingBubbleDialogView::Type type);
  void Hide();

  PriceTrackingBubbleDialogView* GetBubble() const;

 private:
  bool IsShowing();

  const raw_ptr<views::View, DanglingUntriaged> anchor_view_;
  views::ViewTracker tracker_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
  base::OnceClosure on_dialog_closing_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_BUBBLE_DIALOG_VIEW_H_
