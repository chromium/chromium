// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/side_by_side_button.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

SideBySideToolbarButton::SideBySideToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&SideBySideToolbarButton::ButtonPressed,
                                        base::Unretained(this)),
                    nullptr,
                    nullptr),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey,
              kToolbarSideBySideToolbarButtonElementId);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_BY_SIDE));

  SetVectorIcon(vector_icons::kCelebrationIcon);
  SetVisible(false);
  browser->tab_strip_model()->AddObserver(this);
}

SideBySideToolbarButton::~SideBySideToolbarButton() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void SideBySideToolbarButton::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  UpdateButtonVisibility();
}

void SideBySideToolbarButton::OnSplitViewAdded(std::vector<int> indices) {
  UpdateButtonVisibility();
}

void SideBySideToolbarButton::ButtonPressed(const ui::Event& event) {}

void SideBySideToolbarButton::UpdateButtonVisibility() {
  if (browser_->tab_strip_model() &&
      browser_->tab_strip_model()->GetActiveTab()) {
    SetVisible(browser_->tab_strip_model()->GetActiveTab()->IsSplit());
  } else {
    SetVisible(false);
  }
}

BEGIN_METADATA(SideBySideToolbarButton)
END_METADATA
