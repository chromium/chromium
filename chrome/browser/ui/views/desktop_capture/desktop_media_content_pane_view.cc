// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_content_pane_view.h"

#include <memory>
#include <string_view>

#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

std::unique_ptr<views::View> CreateAudioRecommendationView() {
  auto recommendation_view = std::make_unique<views::View>();
  recommendation_view->SetBackground(
      views::CreateRoundedRectBackground(ui::kColorSysSurface1, 8));
  recommendation_view->SetBorder(
      views::CreateRoundedRectBorder(1, 8, ui::kColorSysNeutralOutline));
  recommendation_view->SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(8, 16, 0, 16));

  auto* rec_layout =
      recommendation_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(8, 12)));
  rec_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  rec_layout->set_between_child_spacing(8);

  auto* rec_icon =
      recommendation_view->AddChildView(std::make_unique<views::ImageView>());
  rec_icon->SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kInfoIcon,
                                                    ui::kColorIcon, 16));

  auto* rec_label = recommendation_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_DISPLAY_MEDIA_PICKER_AUDIO_RECOMMENDED)));
  rec_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  return recommendation_view;
}

}  // namespace

DesktopMediaContentPaneView::DesktopMediaContentPaneView(
    std::unique_ptr<views::View> content_view,
    std::unique_ptr<ShareAudioView> share_audio_view,
    bool show_audio_recommendation,
    AudioSharingToggleStyle style_audio_toggle) {
  float bottom_radius = 8;
  SetBackground(views::CreateRoundedRectBackground(ui::kColorSysSurface4,
                                                   /*top_radius=*/0,
                                                   bottom_radius));
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(0)));
  layout->SetFlexForView(AddChildView(std::move(content_view)), 1);

  if (!share_audio_view) {
    SetPaintToLayer();
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(0, 0, bottom_radius, bottom_radius));
    return;
  }

  View* separator_container = AddChildView(std::make_unique<views::View>());
  separator_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      style_audio_toggle == AudioSharingToggleStyle::kBoxed
          ? gfx::Insets::TLBR(12, 16, 0, 16)
          : gfx::Insets::VH(0, 16)));
  separator_container->AddChildView(std::make_unique<views::Separator>());

  if (show_audio_recommendation) {
    audio_recommendation_view_ = AddChildView(CreateAudioRecommendationView());
  }

#if BUILDFLAG(IS_MAC)
  audio_warning_view_ =
      AddChildView(std::make_unique<AudioPermissionWarningView>(
          base::BindRepeating(&DesktopMediaContentPaneView::CancelAudioSharing,
                              base::Unretained(this))));
  audio_warning_view_->SetVisible(false);
#endif  // BUILDFLAG(IS_MAC)

  share_audio_view_ = AddChildView(std::move(share_audio_view));
}

DesktopMediaContentPaneView::~DesktopMediaContentPaneView() = default;

bool DesktopMediaContentPaneView::IsAudioRecommendationVisible() const {
  return audio_recommendation_view_ && audio_recommendation_view_->GetVisible();
}

void DesktopMediaContentPaneView::SetAudioRecommendationVisible(bool visible) {
  if (audio_recommendation_view_) {
    audio_recommendation_view_->SetVisible(visible);
    InvalidateLayout();
  }
}

bool DesktopMediaContentPaneView::AudioOffered() const {
  return share_audio_view_ && share_audio_view_->AudioOffered();
}

bool DesktopMediaContentPaneView::IsAudioSharingApprovedByUser() const {
  return share_audio_view_ && share_audio_view_->IsAudioSharingApprovedByUser();
}

void DesktopMediaContentPaneView::SetAudioSharingApprovedByUser(bool is_on) {
  CHECK(share_audio_view_);
  share_audio_view_->SetAudioSharingApprovedByUser(is_on);
}

std::u16string_view DesktopMediaContentPaneView::GetAudioLabelText() const {
  return share_audio_view_ ? share_audio_view_->GetAudioLabelText()
                           : std::u16string_view();
}

#if BUILDFLAG(IS_MAC)
void DesktopMediaContentPaneView::SetAudioWarningVisible(bool visible) {
  if (audio_warning_view_) {
    audio_warning_view_->SetWarningVisible(visible);
    if (!visible) {
      share_audio_view_->RequestFocus();
    }
  }
}

bool DesktopMediaContentPaneView::IsAudioWarningVisible() const {
  return audio_warning_view_ && audio_warning_view_->GetVisible();
}

void DesktopMediaContentPaneView::CancelAudioSharing() {
  SetAudioSharingApprovedByUser(false);
  SetAudioWarningVisible(false);
}
#endif  // BUILDFLAG(IS_MAC)

BEGIN_METADATA(DesktopMediaContentPaneView)
END_METADATA
