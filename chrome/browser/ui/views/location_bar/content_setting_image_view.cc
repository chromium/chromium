// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

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
  content_setting_image_model_->UpdateFromWebContents(web_contents);

  if (!content_setting_image_model_->is_visible()) {
    SetVisible(false);
    return;
  }

  UpdateImage();
  SetVisible(true);

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

bool ContentSettingImageView::GetTooltipText(const gfx::Point& p,
                                             base::string16* tooltip) const {
  *tooltip = content_setting_image_model_->get_tooltip();
  return !tooltip->empty();
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
  if (GetKeyClickActionForEvent(event) == KeyClickAction::CLICK_ON_KEY_RELEASE)
    PauseAnimation();
  return Button::OnKeyPressed(event);
}

void ContentSettingImageView::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  UpdateImage();
  IconLabelBubbleView::OnNativeThemeChanged(native_theme);
}

SkColor ContentSettingImageView::GetTextColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

bool ContentSettingImageView::ShouldShowSeparator() const {
  return false;
}

bool ContentSettingImageView::ShowBubble(const ui::Event& event) {
  PauseAnimation();
  content::WebContents* web_contents =
      delegate_->GetContentSettingWebContents();
  if (web_contents && !bubble_view_) {
    views::View* const anchor = parent();
    bubble_view_ = new ContentSettingBubbleContents(
        content_setting_image_model_->CreateBubbleModel(
            delegate_->GetContentSettingBubbleModelDelegate(), web_contents,
            Profile::FromBrowserContext(web_contents->GetBrowserContext())),
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
