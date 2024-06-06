// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/result_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace quick_answers {
namespace {
// Phonetics audio button.
// TODO(b/335701090): Use LayoutProvider.
constexpr auto kPhoneticsAudioButtonMarginInsets =
    gfx::Insets::TLBR(0, 4, 0, 4);
constexpr int kPhoneticsAudioButtonSizeDip = 14;
constexpr int kPhoneticsAudioButtonBorderDip = 3;

constexpr int kItemSpacing = 4;

views::Builder<views::ImageButton> PhoneticsAudioButton() {
  return views::Builder<views::ImageButton>()
      .SetImageModel(
          views::Button::ButtonState::STATE_NORMAL,
          ui::ImageModel::FromVectorIcon(vector_icons::kVolumeUpIcon,
                                         ui::kColorButtonBackgroundProminent,
                                         kPhoneticsAudioButtonSizeDip))
      .SetTooltipText(l10n_util::GetStringUTF16(
          IDS_RICH_ANSWERS_VIEW_PHONETICS_BUTTON_A11Y_NAME_TEXT))
      .SetBorder(views::CreateEmptyBorder(kPhoneticsAudioButtonBorderDip));
}

bool IsEmpty(PhoneticsInfo phonetics_info) {
  return phonetics_info.phonetics_audio.is_empty();
}

}  // namespace

ResultView::ResultView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetDefault(views::kMarginsKey, gfx::Insets::VH(kItemSpacing, 0));
  SetCollapseMargins(true);

  views::Label* first_line_label;
  views::Label* first_line_sub_label;
  views::ImageButton* phonetics_audio_button;
  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&first_line_label)
                  .SetVisible(false)
                  .SetEnabledColorId(ui::kColorLabelForeground)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kPreferred)))
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&first_line_sub_label)
                  .SetVisible(false)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetEnabledColorId(ui::kColorLabelForeground))
          .AddChild(PhoneticsAudioButton()
                        .CopyAddressTo(&phonetics_audio_button)
                        .SetVisible(false)
                        .SetProperty(views::kMarginsKey,
                                     kPhoneticsAudioButtonMarginInsets)
                        .SetCallback(base::BindRepeating(
                            &ResultView::OnPhoneticsAudioButtonPressed,
                            base::Unretained(this))))
          .Build());

  CHECK(first_line_label);
  first_line_label_ = first_line_label;
  CHECK(first_line_sub_label);
  first_line_sub_label_ = first_line_sub_label;
  CHECK(phonetics_audio_button);
  phonetics_audio_button_ = phonetics_audio_button;

  views::Label* second_line_label;
  AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&second_line_label)
          .SetVisible(false)
          .SetEnabledColorId(ui::kColorLabelForegroundSecondary)
          .SetMultiLine(true)
          .SetMaxLines(kMaxLines)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kScaleToZero,
                           views::MaximumFlexSizeRule::kPreferred, true))
          .Build());

  CHECK(second_line_label);
  second_line_label_ = second_line_label;
}

ResultView::~ResultView() = default;

void ResultView::SetFirstLineText(const std::u16string& first_line_text) {
  first_line_label_->SetText(first_line_text);
  first_line_label_->SetVisible(!first_line_text.empty());
}

std::u16string ResultView::GetFirstLineText() const {
  return first_line_label_->GetText();
}

void ResultView::SetFirstLineSubText(
    const std::u16string& first_line_sub_text) {
  first_line_sub_label_->SetText(first_line_sub_text);
  first_line_sub_label_->SetVisible(!first_line_sub_text.empty());
}

std::u16string ResultView::GetFirstLineSubText() const {
  return first_line_sub_label_->GetText();
}

void ResultView::SetPhoneticsInfo(const PhoneticsInfo& phonetics_info) {
  phonetics_info_ = phonetics_info;
  phonetics_audio_button_->SetVisible(!IsEmpty(phonetics_info_));
}

PhoneticsInfo ResultView::GetPhoneticsInfo() const {
  return phonetics_info_;
}

void ResultView::SetSecondLineText(const std::u16string& second_line_text) {
  second_line_label_->SetText(second_line_text);
  second_line_label_->SetVisible(!second_line_text.empty());
}

std::u16string ResultView::GetSecondLineText() const {
  return second_line_label_->GetText();
}

void ResultView::SetGenerateTtsCallback(
    GenerateTtsCallback generate_tts_callback) {
  generate_tts_callback_ = generate_tts_callback;
}

std::u16string ResultView::GetA11yDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_DESC_TEMPLATE_V2,
      first_line_label_->GetText(), second_line_label_->GetText());
}

void ResultView::OnPhoneticsAudioButtonPressed() {
  CHECK(!IsEmpty(phonetics_info_));
  CHECK(!generate_tts_callback_.is_null());
  generate_tts_callback_.Run(phonetics_info_);
}

BEGIN_METADATA(ResultView)
ADD_PROPERTY_METADATA(std::u16string, FirstLineText)
ADD_PROPERTY_METADATA(std::u16string, FirstLineSubText)
ADD_PROPERTY_METADATA(std::u16string, SecondLineText)
ADD_READONLY_PROPERTY_METADATA(std::u16string, A11yDescription)
END_METADATA

}  // namespace quick_answers
