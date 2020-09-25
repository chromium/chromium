// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_controller_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"

MediaToolbarButtonView::MediaToolbarButtonView(BrowserView* browser_view)
    : ToolbarButton(this),
      browser_(browser_view->browser()),
      service_(MediaNotificationServiceFactory::GetForProfile(
          browser_view->browser()->profile())),
      feature_promo_controller_(browser_view->feature_promo_controller()) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  EnableCanvasFlippingForRTLUI(false);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_ICON_TOOLTIP_TEXT));
  GetViewAccessibility().OverrideHasPopup(ax::mojom::HasPopup::kDialog);

  // We start hidden and only show once |controller_| tells us to.
  SetVisible(false);

  // Wait until we're done with everything else before creating |controller_|
  // since it can call |Show()| synchronously.
  controller_ = std::make_unique<MediaToolbarButtonController>(this, service_);
}

MediaToolbarButtonView::~MediaToolbarButtonView() {
  // When |controller_| is destroyed, it may call
  // |MediaToolbarButtonView::Hide()|, so we want to be sure it's destroyed
  // before any of our other members.
  controller_.reset();
}

void MediaToolbarButtonView::AddObserver(MediaToolbarButtonObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaToolbarButtonView::RemoveObserver(
    MediaToolbarButtonObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaToolbarButtonView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else {
    MediaDialogView::ShowDialog(this, service_);

    feature_promo_controller_->CloseBubble(
        feature_engagement::kIPHLiveCaptionFeature);

    for (auto& observer : observers_)
      observer.OnMediaDialogOpened();
  }
}

void MediaToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();

  for (auto& observer : observers_)
    observer.OnMediaButtonShown();
}

void MediaToolbarButtonView::Hide() {
  SetVisible(false);
  PreferredSizeChanged();

  for (auto& observer : observers_)
    observer.OnMediaButtonHidden();
}

void MediaToolbarButtonView::Enable() {
  SetEnabled(true);

  if (base::FeatureList::IsEnabled(media::kLiveCaption)) {
    feature_promo_controller_->MaybeShowPromo(
        feature_engagement::kIPHLiveCaptionFeature);
  }

  for (auto& observer : observers_)
    observer.OnMediaButtonEnabled();
}

void MediaToolbarButtonView::Disable() {
  SetEnabled(false);

  feature_promo_controller_->CloseBubble(
      feature_engagement::kIPHLiveCaptionFeature);

  for (auto& observer : observers_)
    observer.OnMediaButtonDisabled();
}

void MediaToolbarButtonView::UpdateIcon() {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  const gfx::VectorIcon& icon =
      touch_ui ? kMediaToolbarButtonTouchIcon : kMediaToolbarButtonIcon;
  UpdateIconsWithStandardColors(icon);
}
