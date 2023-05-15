// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"

namespace quick_answers {

// QuickTextLabel -----------------------------------------------------------

QuickAnswersTextLabel::QuickAnswersTextLabel(
    const QuickAnswerText& quick_answers_text)
    : views::Label(quick_answers_text.text) {
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  SetEnabledColorId(quick_answers_text.color_id);
}

BEGIN_METADATA(QuickAnswersTextLabel, views::Label)
END_METADATA

}  // namespace quick_answers
