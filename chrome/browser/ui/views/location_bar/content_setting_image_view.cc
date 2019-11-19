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
    case ImageType::PPAPI_BROKER:
    case ImageType::PLUGINS:
    case ImageType::GEOLOCATION:
    case ImageType::MIXEDSCRIPT:
    case ImageType::PROTOCOL_HANDLERS:
    case ImageType::MEDIASTREAM:
    case ImageType::ADS:
    case ImageType::AUTOMATIC_DOWNLOADS:
    case ImageType::MIDI_SYSEX:
    case ImageType::SOUND:
    case ImageType::FRAMEBUST:
    case ImageType::CLIPBOARD_READ:
    case ImageType::SENSORS:
    case ImageType::NOTIFICATIONS_QUIET_PROMPT:
      return base::nullopt;

    case ImageType::NUM_IMAGE_TYPES:
      break;
  }
  NOTREACHED();
  return base::nullopt;
}

}  // namespace

ContentSettingImageView::ContentSettingImageView(
    std::unique_ptr<ContentSettingImageModel> image_model,
    Delegate* delegate,
    const gfx::FontList& font_list)
    : IconLabelBubbleView(font_list),
      delegate_(delegate),
      content_setting_image_model_(std::move(image_model)),
      bubble_view_(nullptr) {
  DCHECK(delegate_);
  SetUpForInOutAnimation();
  image()->EnableCanvasFlippingForRTLUI(true);

  base::Optional<ViewID> view_id =
      GetViewID(content_setting_image_model_->image_type());
  if (view_id)
    SetID(*view_id);
}

ContentSettingImageView::~ContentSettingImageView() {
  if (bubble_view_ && bubble_view_->GetWidget())
    bubble_view_->GetWidget()->RemoveObserver(this);
}

void ContentSettingImageView::Update() {
  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();
  // Note: We explicitly want to call this even if |web_contents| is NULL, so we
  // get hidden properly while the user is editing the omnibox.
  content_setting_image_model_->Update(web_contents);
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

void ContentSettingImageView::SetIconColor(SkColor color) {
  icon_color_ = color;
  if (content_setting_image_model_->is_visible())
    UpdateImage();
}

const char* ContentSettingImageView::GetClassName() const {
  return "ContentSettingsImageView";
}

void ContentSettingImageView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  if (bubble_view_)
    bubble_view_->OnAnchorBoundsChanged();
  IconLabelBubbleView::OnBoundsChanged(previous_bounds);
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
  if (GetKeyClickActionForEvent(event) == KeyClickAction::kOnKeyRelease)
    PauseAnimation();
  return Button::OnKeyPressed(event);
}

void ContentSettingImageView::OnThemeChanged() {
  UpdateImage();
  IconLabelBubbleView::OnThemeChanged();
}

SkColor ContentSettingImageView::GetTextColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
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
    bubble_widget->AddObserver(this);
    bubble_widget->Show();
    delegate_->OnContentSettingImageBubbleShown(
        content_setting_image_model_->image_type());
  }

  return true;
}

bool ContentSettingImageView::IsBubbleShowing() const {
  return bubble_view_ != nullptr;
}

SkColor ContentSettingImageView::GetInkDropBaseColor() const {
  return delegate_->GetContentSettingInkDropColor();
}

ContentSettingImageModel::ImageType ContentSettingImageView::GetTypeForTesting()
    const {
  return content_setting_image_model_->image_type();
}

void ContentSettingImageView::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(bubble_view_);
  DCHECK_EQ(bubble_view_->GetWidget(), widget);
  widget->RemoveObserver(this);
  bubble_view_ = nullptr;
  UnpauseAnimation();
}

void ContentSettingImageView::UpdateImage() {
  SetImage(content_setting_image_model_
               ->GetIcon(icon_color_ ? icon_color_.value()
                                     : color_utils::DeriveDefaultIconColor(
                                           GetTextColor()))
               .AsImageSkia());
}
