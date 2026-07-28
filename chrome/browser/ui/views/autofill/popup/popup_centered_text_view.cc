// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_centered_text_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {
constexpr int kHeight = 48;
}  // namespace

PopupCenteredTextView::PopupCenteredTextView(const std::u16string& message) {
  SetUseDefaultFillLayout(true);
  AddChildView(
      views::Builder<views::Label>()
          .SetText(message)
          .SetEnabledColor(ui::kColorLabelForegroundSecondary)
          .SetTextStyle(views::style::TextStyle::STYLE_BODY_4)
          .SetLineHeight(kHeight)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
          .Build());
}

PopupCenteredTextView::~PopupCenteredTextView() = default;

BEGIN_METADATA(PopupCenteredTextView)
END_METADATA

}  // namespace autofill
