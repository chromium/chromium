// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_definition_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace quick_answers {

// RichAnswersDefinitionView
// -----------------------------------------------------------

RichAnswersDefinitionView::RichAnswersDefinitionView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    const quick_answers::QuickAnswer& result)
    : RichAnswersView(anchor_view_bounds, controller, result) {
  InitLayout();

  // TODO (b/274184670): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersDefinitionView::~RichAnswersDefinitionView() = default;

void RichAnswersDefinitionView::InitLayout() {
  // TODO (b/265254908): Populate definition view contents.
}

BEGIN_METADATA(RichAnswersDefinitionView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
