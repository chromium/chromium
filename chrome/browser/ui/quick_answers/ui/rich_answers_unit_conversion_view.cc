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
#include "ui/color/color_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"

namespace quick_answers {

namespace {

void AddAlternativeUnits(views::View* container_view,
                         const UnitConversionResult& unit_conversion_result) {
  for (const UnitConversion& unit_conversion :
       unit_conversion_result.alternative_unit_conversions_list) {
    double dest_amount = unit_conversion.ConvertSourceAmountToDestAmount(
        unit_conversion_result.source_amount);

    std::string dest_amount_text =
        BuildRoundedUnitAmountDisplayText(dest_amount);

    views::BoxLayoutView* box_layout_view =
        container_view->AddChildView(CreateHorizontalBoxLayoutView());
    box_layout_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
        dest_amount_text, GetFontList(TypographyToken::kCrosButton1),
        kContentTextWidth,
        /*is_multi_line=*/false, ui::kColorSysOnSurface));
    box_layout_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
        unit_conversion.dest_rule().unit_name(),
        GetFontList(TypographyToken::kCrosBody2), kContentTextWidth,
        /*is_multi_line=*/false, ui::kColorSysSecondary));
  }
}

}  // namespace

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

  if (!unit_conversion_result_.alternative_unit_conversions_list.empty()) {
    content_view_->AddChildView(CreateSeparatorView());

    AddAlternativeUnits(content_view_, unit_conversion_result_);
  }
}

void RichAnswersUnitConversionView::AddConversionResultText() {
  content_view_->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      unit_conversion_result_.result_text,
      GetFontList(TypographyToken::kCrosDisplay5), kContentTextWidth,
      /*is_multi_line=*/true, ui::kColorSysOnSurface));
}

void RichAnswersUnitConversionView::MaybeAddFormulaInformation() {
  if (!unit_conversion_result_.source_to_dest_unit_conversion.has_value()) {
    return;
  }

  UnitConversion unit_conversion =
      unit_conversion_result_.source_to_dest_unit_conversion.value();
  std::optional<std::string> formula_description_text =
      unit_conversion.GetConversionFormulaText();
  if (!formula_description_text) {
    return;
  }

  content_view_->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      l10n_util::GetStringUTF8(
          IDS_RICH_ANSWERS_VIEW_UNIT_CONVERSION_FORMULA_LABEL_TEXT),
      GetFontList(TypographyToken::kCrosBody2Italic), kContentTextWidth,
      /*is_multi_line=*/false, ui::kColorSysSecondary));

  views::BoxLayoutView* subcontent_view =
      content_view_->AddChildView(CreateHorizontalBoxLayoutView());
  subcontent_view->SetInsideBorderInsets(kSubContentViewInsets);
  subcontent_view->AddChildView(QuickAnswersTextLabel::CreateLabelWithStyle(
      formula_description_text.value(),
      GetFontList(TypographyToken::kCrosBody2), kContentTextWidth,
      /*is_multi_line=*/true, ui::kColorSysOnSurface));
}

BEGIN_METADATA(RichAnswersUnitConversionView)
END_METADATA

}  // namespace quick_answers
