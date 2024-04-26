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
