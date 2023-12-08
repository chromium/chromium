// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_unit_conversion_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"

namespace quick_answers {

// RichAnswersUnitConversionView
// -----------------------------------------------------------

RichAnswersUnitConversionView::RichAnswersUnitConversionView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    UnitConversionResult& unit_conversion_result)
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
  // TODO (b/265257940): Populate unit conversion view contents.
  content_view_ = GetContentView();

  AddHeaderViewsTo(content_view_, unit_conversion_result_.result_text);
}

BEGIN_METADATA(RichAnswersUnitConversionView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
