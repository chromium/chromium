// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_EMAIL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_EMAIL_DIALOG_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace views {
class StyledLabel;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kPriceTrackingEmailConsentDialogId);

class PriceTrackingEmailDialogView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(PriceTrackingEmailDialogView, LocationBarBubbleDelegateView)

 public:
  PriceTrackingEmailDialogView(View* anchor_view,
                               content::WebContents* web_contents,
                               Profile* profile);
  ~PriceTrackingEmailDialogView() override;

 private:
  void OpenHelpArticle();
  void OpenSettings();
  void OnAccepted();
  void OnCanceled();
  void OnClosed();

  const raw_ptr<Profile> profile_;
  raw_ptr<views::StyledLabel> body_label_;
  raw_ptr<views::StyledLabel> help_label_;

  base::WeakPtrFactory<PriceTrackingEmailDialogView> weak_factory_{this};
};

class PriceTrackingEmailDialogCoordinator : public views::WidgetObserver {
 public:
  explicit PriceTrackingEmailDialogCoordinator(views::View* anchor_view);
  ~PriceTrackingEmailDialogCoordinator() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void Show(content::WebContents* web_contents,
            Profile* profile,
            base::OnceClosure on_dialog_closing_callback);
  void Hide();

  PriceTrackingEmailDialogView* GetBubble() const;

 private:
  bool IsShowing();

  const raw_ptr<views::View> anchor_view_;
  views::ViewTracker tracker_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};
  base::OnceClosure on_dialog_closing_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_PRICE_TRACKING_EMAIL_DIALOG_VIEW_H_
