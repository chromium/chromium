// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include <memory>
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"

#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

PopupRowContentView::PopupRowContentView() {
  SetNotifyEnterExitOnChild(true);
  UpdateStyle(false);
}

PopupRowContentView::~PopupRowContentView() = default;

void PopupRowContentView::TrackLabel(views::Label* label) {
  tracked_labels_.push_back(label);
}

void PopupRowContentView::UpdateStyle(bool selected) {
  if (selected) {
    if (ShouldApplyNewAutofillPopupStyle()) {
      SetBackground(views::CreateThemedRoundedRectBackground(
          ui::kColorDropdownBackgroundSelected,
          ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
              views::Emphasis::kMedium)));
    } else {
      SetBackground(views::CreateThemedSolidBackground(
          ui::kColorDropdownBackgroundSelected));
    }
  } else {
    SetBackground(nullptr);
  }

  // Set style for each label in this cell depending on its current selection
  // state.
  for (views::Label* label : tracked_labels_) {
    label->SetAutoColorReadabilityEnabled(false);

    // If the current suggestion is selected or the label is disabled,
    // override the style. Otherwise, use the color that corresponds to the
    // actual style of the label.
    int style = label->GetEnabled() ? (selected ? views::style::STYLE_SELECTED
                                                : label->GetTextStyle())
                                    : views::style::STYLE_DISABLED;
    label->SetEnabledColorId(views::TypographyProvider::Get().GetColorId(
        label->GetTextContext(), style));
  }

  SchedulePaint();
}

BEGIN_METADATA(PopupRowContentView, views::View)
END_METADATA

}  // namespace autofill
