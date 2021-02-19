// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include <utility>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace {

base::Optional<ViewID> GetViewID(
    ContentSettingImageModel::ImageType image_type) {
  using ImageType = ContentSettingImageModel::ImageType;
  switch (image_type) {
    case ImageType::JAVASCRIPT:
      return ViewID::VIEW_ID_CONTENT_SETTING_JAVASCRIPT;

    case ImageType::POPUPS:
      return ViewID::VIEW_ID_CONTENT_SETTING_POPUP;

    case ImageType::COOKIES:
    case ImageType::IMAGES:
    case ImageType::GEOLOCATION:
    case ImageType::MIXEDSCRIPT:
    case ImageType::PROTOCOL_HANDLERS:
    case ImageType::MEDIASTREAM:
    case ImageType::ADS:
    case ImageType::AUTOMATIC_DOWNLOADS:
    case ImageType::MIDI_SYSEX:
    case ImageType::SOUND:
    case ImageType::FRAMEBUST:
    case ImageType::CLIPBOARD_READ_WRITE:
    case ImageType::SENSORS:
    case ImageType::NOTIFICATIONS_QUIET_PROMPT:
      return base::nullopt;

    case ImageType::NUM_IMAGE_TYPES:
      break;
  }
  NOTREACHED();
  return base::nullopt;
}

// The preferred max width for the promo to be shown.
const unsigned int promo_width = 240;

}  // namespace

ContentSettingImageView::ContentSettingImageView(
    std::unique_ptr<ContentSettingImageModel> image_model,
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list, parent_delegate),
      delegate_(delegate),
      content_setting_image_model_(std::move(image_model)),
      bubble_view_(nullptr) {
  DCHECK(delegate_);
  SetUpForInOutAnimation();
  image()->SetFlipCanvasOnPaintForRTLUI(true);

  base::Optional<ViewID> view_id =
      GetViewID(content_setting_image_model_->image_type());
  if (view_id)
    SetID(*view_id);
}

ContentSettingImageView::~ContentSettingImageView() {}

void ContentSettingImageView::Update() {
  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();

  // Calling Update() with a nullptr WebContents will hide the image.
  content_setting_image_model_->Update(
      delegate_->ShouldHideContentSettingImage() ? nullptr : web_contents);
  SetTooltipText(content_setting_image_model_->get_tooltip());

  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }
  DCHECK(web_contents);
  UpdateImage();
  SetVisible(true);

  if (content_setting_image_model_->ShouldNotifyAccessibility(web_contents)) {
    GetViewAccessibility().OverrideName(l10n_util::GetStringUTF16(
        content_setting_image_model_->explanatory_string_id()));
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
    content_setting_image_model_->AccessibilityWasNotified(web_contents);
  }

  if (content_setting_image_model_->ShouldAutoOpenBubble(web_contents)) {
    ShowBubbleImpl();
    content_setting_image_model_->SetBubbleWasAutoOpened(web_contents);
  }

  // If the content usage or blockage should be indicated to the user, start the
  // animation and record that the icon has been shown.
  if (!can_animate_ ||
      !content_setting_image_model_->ShouldRunAnimation(web_contents)) {
    return;
  }

  // We just ignore this blockage if we're already showing some other string to
  // the user.  If this becomes a problem, we could design some sort of queueing
  // mechanism to show one after the other, but it doesn't seem important now.
  int string_id = content_setting_image_model_->explanatory_string_id();
  if (string_id)
    AnimateIn(string_id);

  content_setting_image_model_->SetAnimationHasRun(web_contents);
}

void ContentSettingImageView::SetIconColor(base::Optional<SkColor> color) {
  if (icon_color_ == color)
    return;
  icon_color_ = color;
  if (content_setting_image_model_->is_visible())
    UpdateImage();
  OnPropertyChanged(&icon_color_, views::kPropertyEffectsNone);
}

base::Optional<SkColor> ContentSettingImageView::GetIconColor() const {
  return icon_color_;
}

bool ContentSettingImageView::OnMousePressed(const ui::MouseEvent& event) {
  // Pause animation so that the icon does not shrink and deselect while the
  // user is attempting to press it.
  PauseAnimation();
  return IconLabelBubbleView::OnMousePressed(event);
}

bool ContentSettingImageView::OnKeyPressed(const ui::KeyEvent& event) {
  // Pause animation so that the icon does not shrink and deselect while the
  // user is attempting to press it using key commands.
  if (GetKeyClickActionForEvent(event) == KeyClickAction::kOnKeyRelease) {
    PauseAnimation();
  }
  return Button::OnKeyPressed(event);
}

void ContentSettingImageView::OnThemeChanged() {
  UpdateImage();
  IconLabelBubbleView::OnThemeChanged();
}

bool ContentSettingImageView::ShouldShowSeparator() const {
  return false;
}

bool ContentSettingImageView::ShowBubble(const ui::Event& event) {
  return ShowBubbleImpl();
}

bool ContentSettingImageView::ShowBubbleImpl() {
  PauseAnimation();
  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();
  if (web_contents && !bubble_view_) {
    views::View* const anchor = parent();
    bubble_view_ = new ContentSettingBubbleContents(
        content_setting_image_model_->CreateBubbleModel(
            delegate_->GetContentSettingBubbleModelDelegate(), web_contents),
        web_contents, anchor, views::BubbleBorder::TOP_RIGHT);
    bubble_view_->SetHighlightedButton(this);
    views::Widget* bubble_widget =
        views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
    observation_.Observe(bubble_widget);
    bubble_widget->Show();
    delegate_->OnContentSettingImageBubbleShown(
        content_setting_image_model_->image_type());
  }

  return true;
}

bool ContentSettingImageView::IsBubbleShowing() const {
  return bubble_view_ != nullptr;
}

ContentSettingImageModel::ImageType ContentSettingImageView::GetTypeForTesting()
    const {
  return content_setting_image_model_->image_type();
}

void ContentSettingImageView::OnWidgetDestroying(views::Widget* widget) {
  if (!bubble_view_ || bubble_view_->GetWidget() != widget)
    return;

  DCHECK(observation_.IsObservingSource(widget));
  observation_.Reset();
  bubble_view_ = nullptr;
  UnpauseAnimation();
}

void ContentSettingImageView::UpdateImage() {
  gfx::Image icon = content_setting_image_model_->GetIcon(icon_color_.value_or(
      color_utils::DeriveDefaultIconColor(GetForegroundColor())));
  if (!icon.IsEmpty())
    SetImageModel(ui::ImageModel::FromImage(icon));
}

void ContentSettingImageView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);

  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();

  // The promo currently is only used for Notifications, and it is only shown
  // directly after the animation is shown.
  if (web_contents &&
      content_setting_image_model_->ShouldShowPromo(web_contents)) {
    FeaturePromoBubbleParams bubble_params;
    bubble_params.body_string_specifier =
        IDS_NOTIFICATIONS_QUIET_PERMISSION_NEW_REQUEST_PROMO;
    bubble_params.anchor_view = this;
    bubble_params.arrow = views::BubbleBorder::TOP_RIGHT;
    bubble_params.allow_focus = true;
    bubble_params.persist_on_blur = false;
    bubble_params.preferred_width = promo_width;

    auto* promo_controller = FeaturePromoControllerViews::GetForView(this);
    DCHECK(promo_controller);
    promo_controller->ShowCriticalPromo(bubble_params);
  }
}

BEGIN_METADATA(ContentSettingImageView, IconLabelBubbleView)
ADD_PROPERTY_METADATA(base::Optional<SkColor>, IconColor)
END_METADATA
