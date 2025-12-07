// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/combined_selector_sheet_view.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/combined_selector_views.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CombinedSelectorSheetView,
                                      kCombinedSelectorSheetViewId);

CombinedSelectorSheetView::CombinedSelectorSheetView(
    std::unique_ptr<CombinedSelectorSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {
  SetProperty(views::kElementIdentifierKey, kCombinedSelectorSheetViewId);
}

CombinedSelectorSheetView::~CombinedSelectorSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
CombinedSelectorSheetView::BuildStepSpecificContent() {
  auto* sheet_model = static_cast<CombinedSelectorSheetModel*>(model());
  auto autofocus = sheet_model->dialog_model()->mechanisms.size() == 1
                       ? AutoFocus::kNo
                       : AutoFocus::kYes;
  return std::make_pair(
      std::make_unique<CombinedSelectorListView>(sheet_model, this), autofocus);
}

void CombinedSelectorSheetView::OnRadioButtonChecked(int index) {
  static_cast<CombinedSelectorSheetModel*>(model())->SetSelectionIndex(index);
}

BEGIN_METADATA(CombinedSelectorSheetView) END_METADATA
