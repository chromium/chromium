// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"

#include <utility>

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_no_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr gfx::Insets kNoTabsInteriorMargins = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr int kSpacingBetweenChildren = 2;
}  // namespace

ProjectsPanelTabGroupsView::ProjectsPanelTabGroupsView(
    actions::ActionItem* root_action_item,
    views::ActionViewController* action_view_controller,
    ProjectsPanelTabGroupsItemView::TabGroupPressedCallback
        tab_group_button_callback,
    ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback
        more_button_callback)
    : tab_group_button_callback_(std::move(tab_group_button_callback)),
      more_button_callback_(std::move(more_button_callback)) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->set_between_child_spacing(kSpacingBetweenChildren);

  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelTabGroupsViewElementId);
}

ProjectsPanelTabGroupsView::~ProjectsPanelTabGroupsView() = default;

void ProjectsPanelTabGroupsView::SetTabGroups(
    const std::vector<tab_groups::SavedTabGroup>& tab_groups) {
  no_tab_groups_view_ = nullptr;
  RemoveAllChildViews();

  for (const auto& group : tab_groups) {
    AddChildView(std::make_unique<ProjectsPanelTabGroupsItemView>(
        group, tab_group_button_callback_, more_button_callback_));
  }

  if (tab_groups.empty()) {
    no_tab_groups_view_ =
        AddChildView(std::make_unique<ProjectsPanelNoTabGroupsView>());
    no_tab_groups_view_->SetProperty(views::kMarginsKey,
                                     kNoTabsInteriorMargins);
  }
}

BEGIN_METADATA(ProjectsPanelTabGroupsView)
END_METADATA
