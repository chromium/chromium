// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"

#include "base/feature_list.h"
#include "base/strings/pattern.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
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
#include "ui/views/metadata/metadata_impl_macros.h"

MediaToolbarButtonView::MediaToolbarButtonView(BrowserView* browser_view)
    : ToolbarButton(base::BindRepeating(&MediaToolbarButtonView::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser_view->browser()),
      service_(MediaNotificationServiceFactory::GetForProfile(
          browser_view->browser()->profile())),
      feature_promo_controller_(browser_view->feature_promo_controller()) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetFlipCanvasOnPaintForRTLUI(false);
  SetVectorIcons(kMediaToolbarButtonIcon, kMediaToolbarButtonTouchIcon);
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
    // Live Caption multi language is only enabled when SODA is also enabled.
    if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage) &&
        base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
      feature_promo_controller_->MaybeShowPromo(
          feature_engagement::kIPHLiveCaptionFeature);
    } else {
      // Live Caption only works for English-language speech for now, so we only
      // show the promo to users whose fluent languages include english. Fluent
      // languages are set in chrome://settings/languages.
      language::LanguageModel* language_model =
          LanguageModelManagerFactory::GetForBrowserContext(browser_->profile())
              ->GetPrimaryModel();
      for (const auto& lang : language_model->GetLanguages()) {
        if (base::MatchPattern(lang.lang_code, "en*")) {
          feature_promo_controller_->MaybeShowPromo(
              feature_engagement::kIPHLiveCaptionFeature);
          break;
        }
      }
    }
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

void MediaToolbarButtonView::ButtonPressed() {
  if (MediaDialogView::IsShowing()) {
    MediaDialogView::HideDialog();
  } else {
    MediaDialogView::ShowDialog(this, service_, browser_->profile(),
                                GlobalMediaControlsEntryPoint::kToolbarIcon);

    feature_promo_controller_->CloseBubble(
        feature_engagement::kIPHLiveCaptionFeature);

    for (auto& observer : observers_)
      observer.OnMediaDialogOpened();
  }
}

BEGIN_METADATA(MediaToolbarButtonView, ToolbarButton)
END_METADATA
