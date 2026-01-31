// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"

#include "base/notimplemented.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr gfx::Insets kTitleInteriorMargins = gfx::Insets::VH(12, 8);
}  // namespace

ProjectsPanelTabGroupsView::ProjectsPanelTabGroupsView(
    actions::ActionItem* root_action_item,
    views::ActionViewController* action_view_controller,
    ProjectsPanelController* projects_panel_controller) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));
  SetBackground(views::CreateSolidBackground(ui::kColorFrameActive));
  projects_panel_controller_observer_.Observe(projects_panel_controller);

  title_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_TAB_GROUPS_TITLE)));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetTextStyle(views::style::STYLE_HEADLINE_5);
  title_->SetProperty(views::kMarginsKey, kTitleInteriorMargins);

  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelTabGroupsViewElementId);
}

ProjectsPanelTabGroupsView::~ProjectsPanelTabGroupsView() = default;

void ProjectsPanelTabGroupsView::OnTabGroupsInitialized(
    const std::vector<tab_groups::SavedTabGroup>& tab_groups) {
  for (const auto& group : tab_groups) {
    item_views_.emplace_back(
        AddChildView(std::make_unique<ProjectsPanelTabGroupsItemView>(group)));
  }
}

void ProjectsPanelTabGroupsView::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group) {
  NOTIMPLEMENTED();
}

void ProjectsPanelTabGroupsView::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group) {
  NOTIMPLEMENTED();
}

void ProjectsPanelTabGroupsView::OnTabGroupRemoved(const base::Uuid& sync_id) {
  NOTIMPLEMENTED();
}

BEGIN_METADATA(ProjectsPanelTabGroupsView)
END_METADATA
