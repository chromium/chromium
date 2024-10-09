// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"

#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/strings/pattern.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/live_caption/caption_util.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/view_class_properties.h"

MediaToolbarButtonView::MediaToolbarButtonView(
    BrowserView* browser_view,
    std::unique_ptr<MediaToolbarButtonContextualMenu> context_menu)
    : ToolbarButton(base::BindRepeating(&MediaToolbarButtonView::ButtonPressed,
                                        base::Unretained(this)),
                    context_menu ? context_menu->CreateMenuModel() : nullptr,
                    /** tab_strip_model*/ nullptr,
                    /** trigger_menu_on_long_press */ false),
      browser_(browser_view->browser()),
      service_(MediaNotificationServiceFactory::GetForProfile(
          browser_view->browser()->profile())),
      context_menu_(std::move(context_menu)) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetFlipCanvasOnPaintForRTLUI(false);
  SetVectorIcons(kMediaToolbarButtonChromeRefreshIcon,
                 kMediaToolbarButtonTouchIcon);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_ICON_TOOLTIP_TEXT));
  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kDialog);
  SetProperty(views::kElementIdentifierKey, kToolbarMediaButtonElementId);

  // We start hidden and only show once |controller_| tells us to.
  SetVisible(false);

  // Wait until we're done with everything else before creating |controller_|
  // since it can call |Show()| synchronously.
  controller_ = std::make_unique<MediaToolbarButtonController>(
      this, service_->media_item_manager());
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

void MediaToolbarButtonView::Show() {
  SetVisible(true);
  PreferredSizeChanged();

  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonShown);
}

void MediaToolbarButtonView::Hide() {
  SetVisible(false);
  PreferredSizeChanged();

  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonHidden);
}

void MediaToolbarButtonView::Enable() {
  SetEnabled(true);

  // Have to check for browser window because this can be called during setup,
  // before there is a valid widget to anchor anything to. Previously any
  // attempt to display an IPH at this point would have simply failed, so this
  // is not a behavioral change (see crbug.com/1291170).
  if (browser_->window() && captions::IsLiveCaptionFeatureSupported()) {
      browser_->window()->MaybeShowFeaturePromo(
          feature_engagement::kIPHLiveCaptionFeature);
  }

  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonEnabled);
}

void MediaToolbarButtonView::Disable() {
  SetEnabled(false);

  ClosePromoBubble(/*engaged=*/false);

  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonDisabled);
}

void MediaToolbarButtonView::MaybeShowLocalMediaCastingPromo() {
  if (media_router::GlobalMediaControlsCastStartStopEnabled(
          browser_->profile()) &&
      service_->should_show_cast_local_media_iph()) {
    browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHGMCLocalMediaCastingFeature);
  }
}

void MediaToolbarButtonView::MaybeShowStopCastingPromo() {
  if (media_router::GlobalMediaControlsCastStartStopEnabled(
          browser_->profile()) &&
      service_->HasLocalCastNotifications()) {
    browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHGMCCastStartStopFeature);
  }
}

void MediaToolbarButtonView::ButtonPressed() {
  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else {
    MediaDialogView::ShowDialogFromToolbar(this, service_, browser_->profile());
    ClosePromoBubble(/*engaged=*/true);
    observers_.Notify(&MediaToolbarButtonObserver::OnMediaDialogOpened);
  }
}

void MediaToolbarButtonView::ClosePromoBubble(bool engaged) {
  // This can get called during setup before the window is even added to the
  // browser (and before any bubbles could possibly be shown) so if there is no
  // window, just bail.
  if (!browser_->window()) {
    return;
  }

  if (engaged) {
    browser_->window()->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHLiveCaptionFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    browser_->window()->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHGMCCastStartStopFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  } else {
    browser_->window()->AbortFeaturePromo(
        feature_engagement::kIPHLiveCaptionFeature);
    browser_->window()->AbortFeaturePromo(
        feature_engagement::kIPHGMCCastStartStopFeature);
  }
}

BEGIN_METADATA(MediaToolbarButtonView)
END_METADATA
