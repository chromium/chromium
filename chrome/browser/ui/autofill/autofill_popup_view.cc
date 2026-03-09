// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_view.h"

namespace autofill {

AutofillPopupView::TabbedPaneConfig::TabbedPaneConfig(
    std::vector<TabbedPaneConfig::Tab> tabs)
    : tabs(std::move(tabs)) {}

AutofillPopupView::TabbedPaneConfig::TabbedPaneConfig(const TabbedPaneConfig&) =
    default;

AutofillPopupView::TabbedPaneConfig::TabbedPaneConfig(TabbedPaneConfig&&) =
    default;

AutofillPopupView::TabbedPaneConfig&
AutofillPopupView::TabbedPaneConfig::operator=(const TabbedPaneConfig&) =
    default;

AutofillPopupView::TabbedPaneConfig&
AutofillPopupView::TabbedPaneConfig::operator=(TabbedPaneConfig&&) = default;

AutofillPopupView::TabbedPaneConfig::~TabbedPaneConfig() = default;

}  // namespace autofill
