// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"

#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_contextual_menu.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
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
          browser_view->browser()->GetProfile())),
      context_menu_(std::move(context_menu)) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetFlipCanvasOnPaintForRTLUI(false);
  SetVectorIcons(features::IsRoundedIconsEnabled()
                     ? kQueueMusicIcon
                     : kMediaToolbarButtonChromeRefreshOldIcon,
                 features::IsRoundedIconsEnabled()
                     ? kQueueMusicIcon
                     : kMediaToolbarButtonTouchOldIcon);
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

  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonEnabled);
}

void MediaToolbarButtonView::Disable() {
  SetEnabled(false);

  ClosePromoBubble(/*engaged=*/false);

  observers_.Notify(&MediaToolbarButtonObserver::OnMediaButtonDisabled);
}

void MediaToolbarButtonView::MaybeShowLocalMediaCastingPromo() {
  if (service_->should_show_cast_local_media_iph()) {
    BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
        feature_engagement::kIPHGMCLocalMediaCastingFeature);
  }
}

void MediaToolbarButtonView::MaybeShowStopCastingPromo() {
  if (service_->HasLocalCastNotifications()) {
    BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
        feature_engagement::kIPHGMCCastStartStopFeature);
  }
}

void MediaToolbarButtonView::ButtonPressed() {
  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else {
    MediaDialogView::ShowDialogFromToolbar(GetBubbleAnchor(), service_,
                                           browser_->GetProfile());
    ClosePromoBubble(/*engaged=*/true);
    observers_.Notify(&MediaToolbarButtonObserver::OnMediaDialogOpened);
  }
}

void MediaToolbarButtonView::ClosePromoBubble(bool engaged) {
  if (auto* const user_education =
          BrowserUserEducationInterface::From(browser_)) {
    if (engaged) {
      user_education->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHGMCCastStartStopFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    } else {
      user_education->AbortFeaturePromo(
          feature_engagement::kIPHGMCCastStartStopFeature);
    }
  }
}

views::BubbleAnchor MediaToolbarButtonView::GetBubbleAnchor() {
  return views::BubbleAnchor(this);
}

MediaToolbarButtonController* MediaToolbarButtonView::GetController() {
  return controller_.get();
}

BEGIN_METADATA(MediaToolbarButtonView)
END_METADATA
