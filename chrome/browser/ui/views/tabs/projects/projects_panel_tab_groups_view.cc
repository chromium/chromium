// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_no_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr gfx::Insets kNoTabsInteriorMargins = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr int kSpacingBetweenChildren = 2;

class ProjectsPanelNewTabGroupButton : public views::LabelButton {
  METADATA_HEADER(ProjectsPanelNewTabGroupButton, views::LabelButton)

 public:
  explicit ProjectsPanelNewTabGroupButton(base::RepeatingClosure callback)
      : views::LabelButton(
            std::move(callback),
            l10n_util::GetStringUTF16(IDS_CREATE_NEW_TAB_GROUP)) {
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(kCreateNewTabGroupIcon, ui::kColorIcon));
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  ProjectsPanelNewTabGroupButton(const ProjectsPanelNewTabGroupButton&) =
      delete;
  ProjectsPanelNewTabGroupButton& operator=(
      const ProjectsPanelNewTabGroupButton&) = delete;
  ~ProjectsPanelNewTabGroupButton() override = default;
};

BEGIN_METADATA(ProjectsPanelNewTabGroupButton)
END_METADATA

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

  // TODO(crbug.com/481410391): Wire up button to create a new tab group.
  create_new_tab_group_button_ = AddChildView(
      std::make_unique<ProjectsPanelNewTabGroupButton>(base::DoNothing()));
  create_new_tab_group_button_->SetProperty(
      views::kElementIdentifierKey, kProjectsPanelNewTabGroupButtonElementId);

  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelTabGroupsViewElementId);
}

ProjectsPanelTabGroupsView::~ProjectsPanelTabGroupsView() = default;

void ProjectsPanelTabGroupsView::SetTabGroups(
    const std::vector<tab_groups::SavedTabGroup>& tab_groups) {
  // Reset the pointer before removing the view to avoid a dangling pointer.
  no_tab_groups_view_ = nullptr;

  // Remove all children except the new tab group button.
  std::vector<views::View*> children_to_remove;
  for (views::View* child : children()) {
    if (child != create_new_tab_group_button_) {
      children_to_remove.push_back(child);
    }
  }
  for (views::View* child : children_to_remove) {
    RemoveChildViewT(child);
  }

  if (tab_groups.empty()) {
    no_tab_groups_view_ =
        AddChildView(std::make_unique<ProjectsPanelNoTabGroupsView>());
    no_tab_groups_view_->SetProperty(views::kMarginsKey,
                                     kNoTabsInteriorMargins);
  } else {
    for (const auto& group : tab_groups) {
      AddChildView(std::make_unique<ProjectsPanelTabGroupsItemView>(
          group, tab_group_button_callback_, more_button_callback_));
    }
  }
}

BEGIN_METADATA(ProjectsPanelTabGroupsView)
END_METADATA
