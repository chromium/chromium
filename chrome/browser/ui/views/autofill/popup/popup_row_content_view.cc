// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"

#include <algorithm>
#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/common/autofill_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"

namespace autofill {

PopupRowContentView::PopupRowContentView() {
  // The following reasoning applies:
  // * There is padding with distance `PopupRowView::GetHorizontalMargin()`
  //   between the edge of  the Autofill popup row and the start of the content
  //   cell.
  // * In addition, there is also padding inside the content cell. Together,
  //   these two paddings need to add up to
  //   `PopupBaseView::ArrowHorizontalMargin`, since to ensure that the content
  //   inside the content cell is aligned with the popup bubble's arrow.
  //
  //           / \
  //          /   \
  //         /     \
  //        / arrow \
  // ┌─────/         \────────────────────────┐
  // │  ┌──────────────────────────────────┐  │
  // │  │  ┌─────────┐ ┌────────────────┐  │  │
  // ├──┼──┤         │ │                │  │  │
  // ├──┤▲ │  Icon   │ │ Text labels    │  │  │
  // │▲ │| │         │ │                │  │  │
  // ││ ││ └─────────┘ └────────────────┘  │  │
  // ││ └┼─────────────────────────────────┘  │
  // └┼──┼────────────────────────────────────┘
  //  │  │
  //  │  PopupBaseView::ArrowHorizontalMargin()
  //  │
  //  PopupRowView::GetHorizontalMargin()
  SetInsideBorderInsets(
      gfx::Insets::VH(0, std::max(0, PopupBaseView::ArrowHorizontalMargin() -
                                         PopupRowView::GetHorizontalMargin())));
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetNotifyEnterExitOnChild(true);
  UpdateStyle(false);
}

PopupRowContentView::~PopupRowContentView() = default;

void PopupRowContentView::UpdateStyle(bool selected) {
  SetBackground(selected
                    ? views::CreateThemedRoundedRectBackground(
                          ui::kColorDropdownBackgroundSelected,
                          ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                              views::Emphasis::kMedium))
                    : nullptr);
  SchedulePaint();
}

BEGIN_METADATA(PopupRowContentView)
END_METADATA

}  // namespace autofill
