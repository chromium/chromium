// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/share_audio_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

ShareAudioView::ShareAudioView(const std::u16string& label_text,
                               bool audio_offered) {
  SetProperty(views::kMarginsKey, gfx::Insets::TLBR(8, 16, 16, 16));

  views::ImageView* audio_icon_view =
      AddChildView(std::make_unique<views::ImageView>());
  audio_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kVolumeUpIcon,
      audio_offered ? ui::kColorIcon : ui::kColorIconDisabled,
      GetLayoutConstant(PAGE_INFO_ICON_SIZE)));

  audio_toggle_label_ = AddChildView(std::make_unique<views::Label>());
  audio_toggle_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  audio_toggle_label_->SetText(label_text);

  if (audio_offered) {
    audio_toggle_button_ =
        AddChildView(std::make_unique<views::ToggleButton>());
    audio_toggle_button_->GetViewAccessibility().SetName(label_text);
  } else {
    audio_toggle_label_->SetTextStyle(views::style::TextStyle::STYLE_DISABLED);
  }

  views::BoxLayout* audio_toggle_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  audio_toggle_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  audio_toggle_layout->set_between_child_spacing(8);
  audio_toggle_layout->SetFlexForView(audio_toggle_label_, 1);
}

ShareAudioView::~ShareAudioView() = default;

bool ShareAudioView::AudioOffered() const {
  return !!audio_toggle_button_;
}

bool ShareAudioView::IsAudioSharingApprovedByUser() const {
  return audio_toggle_button_ && audio_toggle_button_->GetIsOn();
}

void ShareAudioView::SetAudioSharingApprovedByUser(bool is_on) {
  CHECK(audio_toggle_button_);
  audio_toggle_button_->SetIsOn(is_on);
}

std::u16string ShareAudioView::GetAudioLabelText() const {
  return audio_toggle_label_ ? audio_toggle_label_->GetText()
                             : std::u16string();
}

BEGIN_METADATA(ShareAudioView)
END_METADATA
