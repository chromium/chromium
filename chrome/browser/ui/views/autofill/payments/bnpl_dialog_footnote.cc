// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/bnpl_dialog_footnote.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace autofill::payments {

BnplDialogFootnote::BnplDialogFootnote() {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetInsideBorderInsets(gfx::Insets::TLBR(10, 10, 10, 10));
  AddChildView(
      views::Builder<views::Label>()
          // TODO(crbug.com/356443046): Move to resources and translate string.
          .SetText(u"To hide pay over time options, go to payment settings")
          .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(5, 10, 5, 0))
          .Build());
}

BnplDialogFootnote::~BnplDialogFootnote() = default;

BEGIN_METADATA(BnplDialogFootnote)
END_METADATA

}  // namespace autofill::payments
