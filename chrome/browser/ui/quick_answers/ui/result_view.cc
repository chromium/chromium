// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/result_view.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/ui/typography.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
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
// TODO(b/335701090): Use LayoutProvider for those values.

// Layout code automatically flips left/right margins for RTL.
constexpr auto kPhoneticsAudioButtonMarginInsets =
    gfx::Insets::TLBR(0, 4, 0, 4);
constexpr auto kPhoneticsAudioButtonRefreshedMarginInsets =
    gfx::Insets::TLBR(0, 8, 0, 0);
constexpr int kPhoneticsAudioButtonSizeDip = 14;
constexpr int kPhoneticsAudioButtonBorderDip = 3;
constexpr int kPhoneticsAudioButtonRefreshedBorderDip = 5;
constexpr int kPhoneticsAudioButtonBackgroundRadiusDip = 12;

constexpr int kItemSpacing = 4;

constexpr char16_t kSeparatorText[] = u" · ";

bool IsEmpty(PhoneticsInfo phonetics_info) {
  return phonetics_info.phonetics_audio.is_empty();
}

}  // namespace

ResultView::ResultView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetCollapseMargins(true);

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .CopyAddressTo(&flex_layout_view_)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(0, 0, kItemSpacing, 0))
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&first_line_label_)
                  .SetVisible(false)
                  .SetEnabledColorId(ui::kColorLabelForeground)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  // Set lower priority order for `first_line_label` compared to
                  // `first_line_sub_label` as primary text gets elided first
                  // if a sub text is shown.
                  .SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(
                          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero)
                          .WithOrder(2)))
          .AddChild(views::Builder<views::Label>()
                        .CopyAddressTo(&separator_label_)
                        .SetVisible(false)
                        .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                        .SetEnabledColorId(ui::kColorLabelForeground)
                        .SetText(kSeparatorText))
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&first_line_sub_label_)
                  .SetVisible(false)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetEnabledColorId(ui::kColorLabelForeground)
                  .SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(
                          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero)
                          .WithOrder(1)))
          .AddChild(
              views::Builder<views::ImageButton>()
                  .CopyAddressTo(&phonetics_audio_button_)
                  .SetVisible(false)
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_RICH_ANSWERS_VIEW_PHONETICS_BUTTON_A11Y_NAME_TEXT))
                  .SetCallback(base::BindRepeating(
                      &ResultView::OnPhoneticsAudioButtonPressed,
                      base::Unretained(this))))
          .Build());

  AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&second_line_label_)
          .SetVisible(false)
          .SetEnabledColorId(ui::kColorLabelForegroundSecondary)
          .SetMultiLine(true)
          .SetMaxLines(kMaxLines)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::MinimumFlexSizeRule::kPreferred,
                           views::MaximumFlexSizeRule::kPreferred, true))
          .Build());

  SetDesign(Design::kCurrent);
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
  separator_label_->SetVisible(!first_line_sub_text.empty());
  flex_layout_view_->SetFlexAllocationOrder(
      first_line_sub_text.empty() ? views::FlexAllocationOrder::kNormal
                                  : views::FlexAllocationOrder::kReverse);
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

void ResultView::SetDesign(Design design) {
  first_line_label_->SetFontList(GetFirstLineFontList(design));
  first_line_label_->SetLineHeight(GetFirstLineHeight(design));
  first_line_sub_label_->SetFontList(GetFirstLineFontList(design));
  first_line_sub_label_->SetLineHeight(GetFirstLineHeight(design));

  second_line_label_->SetFontList(GetSecondLineFontList(design));
  second_line_label_->SetLineHeight(GetSecondLineHeight(design));

  phonetics_audio_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kVolumeUpIcon,
                                     design == Design::kCurrent
                                         ? ui::kColorButtonBackgroundProminent
                                         : ui::kColorSysOnSurface,
                                     kPhoneticsAudioButtonSizeDip));
  phonetics_audio_button_->SetBorder(
      design == Design::kCurrent
          ? views::CreateEmptyBorder(kPhoneticsAudioButtonBorderDip)
          : views::CreateEmptyBorder(kPhoneticsAudioButtonRefreshedBorderDip));
  phonetics_audio_button_->SetBackground(
      design == Design::kCurrent
          ? nullptr
          : views::CreateThemedRoundedRectBackground(
                ui::ColorIds::kColorSysStateHoverOnSubtle,
                kPhoneticsAudioButtonBackgroundRadiusDip));
  phonetics_audio_button_->SetProperty(
      views::kMarginsKey, design == Design::kCurrent
                              ? kPhoneticsAudioButtonMarginInsets
                              : kPhoneticsAudioButtonRefreshedMarginInsets);
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
END_METADATA

}  // namespace quick_answers
