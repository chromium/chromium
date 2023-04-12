// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_definition_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/views/layout/fill_layout.h"

// RichAnswersDefinitionView
// -----------------------------------------------------------

RichAnswersDefinitionView::RichAnswersDefinitionView(
    const quick_answers::QuickAnswer& result) {
  InitLayout();

  // TODO (b/274184670): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersDefinitionView::~RichAnswersDefinitionView() = default;

const char* RichAnswersDefinitionView::GetClassName() const {
  return "RichAnswersDefinitionView";
}

void RichAnswersDefinitionView::InitLayout() {
  // TODO (b/265254908): Populate definition view contents.
}
