// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_translation_view.h"

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

// RichAnswersTranslationView
// -----------------------------------------------------------

RichAnswersTranslationView::RichAnswersTranslationView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    TranslationResult& translation_result)
    : RichAnswersView(anchor_view_bounds,
                      controller,
                      ResultType::kTranslationResult),
      translation_result_(translation_result) {
  InitLayout();

  // TODO (b/274184294): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersTranslationView::~RichAnswersTranslationView() = default;

void RichAnswersTranslationView::InitLayout() {
  // TODO (b/265258270): Populate translation view contents.
  content_view_ = GetContentView();

  AddHeaderViewsTo(content_view_, translation_result_.source_locale);
}

BEGIN_METADATA(RichAnswersTranslationView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
