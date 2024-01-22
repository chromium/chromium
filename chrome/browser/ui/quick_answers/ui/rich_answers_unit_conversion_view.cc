// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_unit_conversion_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {

std::string BuildFormulaDescriptionText(double source_rate,
                                        double dest_rate,
                                        const std::string& category) {
  std::u16string arithmetic_operator_text;

  if (source_rate <= dest_rate) {
    arithmetic_operator_text = l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_UNIT_CONVERSION_FORMULA_MULTIPLICATION_OPERATOR_TEXT);
  } else {
    arithmetic_operator_text = l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_UNIT_CONVERSION_FORMULA_DIVISION_OPERATOR_TEXT);
  }

  std::optional<double> conversion_rate =
      quick_answers::GetRatio(source_rate, dest_rate);
  if (!conversion_rate.has_value()) {
    return std::string();
  }

  return l10n_util::GetStringFUTF8(
      IDS_QUICK_ANSWERS_UNIT_CONVERSION_FORMULA_DESCRIPTION_TEXT,
      arithmetic_operator_text, base::UTF8ToUTF16(base::ToLowerASCII(category)),
      base::UTF8ToUTF16(base::StringPrintf(quick_answers::kResultValueTemplate,
                                           conversion_rate.value())));
}

}  // namespace

namespace quick_answers {

// RichAnswersUnitConversionView
// -----------------------------------------------------------

RichAnswersUnitConversionView::RichAnswersUnitConversionView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    const UnitConversionResult& unit_conversion_result)
    : RichAnswersView(anchor_view_bounds,
                      controller,
                      ResultType::kUnitConversionResult),
      unit_conversion_result_(unit_conversion_result) {
  InitLayout();

  // TODO (b/274184290): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersUnitConversionView::~RichAnswersUnitConversionView() = default;

void RichAnswersUnitConversionView::InitLayout() {
  content_view_ = GetContentView();

  AddHeaderViewsTo(content_view_, unit_conversion_result_.source_text);

  AddConversionResultText();

  MaybeAddFormulaInformation();

  // Separator.
  content_view_->AddChildView(CreateSeparatorView());
}

void RichAnswersUnitConversionView::AddConversionResultText() {
  content_view_->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      unit_conversion_result_.result_text,
      GetFontList(TypographyToken::kCrosDisplay5), kContentTextWidth,
      /*is_multi_line=*/true, cros_tokens::kCrosSysOnSurface));
}

void RichAnswersUnitConversionView::MaybeAddFormulaInformation() {
  if (!unit_conversion_result_.standard_unit_conversion_rates.has_value()) {
    return;
  }

  StandardUnitConversionRates conversion_rates =
      unit_conversion_result_.standard_unit_conversion_rates.value();
  std::string formula_description_text = BuildFormulaDescriptionText(
      conversion_rates.source_to_standard_conversion_rate,
      conversion_rates.dest_to_standard_conversion_rate,
      unit_conversion_result_.category);
  if (formula_description_text.empty()) {
    return;
  }

  content_view_->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      l10n_util::GetStringUTF8(
          IDS_QUICK_ANSWERS_UNIT_CONVERSION_FORMULA_LABEL_TEXT),
      GetFontList(TypographyToken::kCrosBody2Italic), kContentTextWidth,
      /*is_multi_line=*/false, cros_tokens::kCrosSysSecondary));

  views::BoxLayoutView* subcontent_view =
      content_view_->AddChildView(CreateHorizontalBoxLayoutView());
  subcontent_view->SetInsideBorderInsets(kSubContentViewInsets);
  subcontent_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      formula_description_text, GetFontList(TypographyToken::kCrosBody2),
      kContentTextWidth,
      /*is_multi_line=*/true, cros_tokens::kCrosSysOnSurface));
}

BEGIN_METADATA(RichAnswersUnitConversionView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
