// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

SplitTabsToolbarButton::SplitTabsToolbarButton(Browser* browser)
    : ToolbarButton(base::BindRepeating(&SplitTabsToolbarButton::ButtonPressed,
                                        base::Unretained(this)),
                    nullptr,
                    nullptr),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey,
              kToolbarSplitTabsToolbarButtonElementId);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SPLIT_TABS));

  SetVectorIcon(kSplitTabIcon);
  SetVisible(false);
  browser->tab_strip_model()->AddObserver(this);
}

SplitTabsToolbarButton::~SplitTabsToolbarButton() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

void SplitTabsToolbarButton::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  UpdateButtonVisibility();
}

void SplitTabsToolbarButton::OnSplitTabCreated(
    std::vector<std::pair<tabs::TabInterface*, int>> tabs,
    split_tabs::SplitTabId split_id,
    TabStripModelObserver::SplitTabAddReason reason,
    tabs::SplitTabLayout tab_layout) {
  UpdateButtonVisibility();
}

void SplitTabsToolbarButton::ButtonPressed(const ui::Event& event) {}

void SplitTabsToolbarButton::UpdateButtonVisibility() {
  if (browser_->tab_strip_model() &&
      browser_->tab_strip_model()->GetActiveTab()) {
    SetVisible(browser_->tab_strip_model()->GetActiveTab()->IsSplit());
  } else {
    SetVisible(false);
  }
}

BEGIN_METADATA(SplitTabsToolbarButton)
END_METADATA
