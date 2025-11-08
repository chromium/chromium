// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"

#include "base/functional/callback.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/collaboration/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_CommentsSidePanelUI =
    SidePanelWebUIViewT<CommentsSidePanelUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_CommentsSidePanelUI,
                        SidePanelWebUIViewT)
END_METADATA

CommentsSidePanelCoordinator::CommentsSidePanelCoordinator(
    BrowserWindowInterface* browser)
    : browser_(browser),
      tab_group_sync_service_(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              browser_->GetProfile())) {
  browser_->GetTabStripModel()->AddObserver(this);
  tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser_->GetProfile())
      ->AddObserver(this);
}

CommentsSidePanelCoordinator::~CommentsSidePanelCoordinator() {
  browser_->GetTabStripModel()->RemoveObserver(this);
  tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser_->GetProfile())
      ->RemoveObserver(this);
}

void CommentsSidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Only handle changing the active tab.
  if (!selection.active_tab_changed()) {
    return;
  }

  UpdateVisuals(selection.new_tab);
}

void CommentsSidePanelCoordinator::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  // Only handle group changes to the active tab.
  if (!tab->IsActivated() || old_group == new_group) {
    return;
  }

  UpdateVisuals(tab);
}

void CommentsSidePanelCoordinator::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  // Only handle updates to the active group.
  std::optional<tab_groups::TabGroupId> active_group_id =
      browser_->GetTabStripModel()->GetActiveTabGroupId();
  if (!active_group_id.has_value() ||
      active_group_id.value() != group.local_group_id()) {
    return;
  }

  UpdateVisuals(browser_->GetActiveTabInterface());
}

void CommentsSidePanelCoordinator::UpdateVisuals(
    const tabs::TabInterface* tab) {
  // Only update the title if change contains a new tab.
  if (tab) {
    UpdateSidePanelTitle(GetSharedTabGroupName(tab));
  }

  const bool should_show_comments_action = ShouldShowCommentsAction(tab);
  UpdateCommentsActionVisibility(should_show_comments_action);
  UpdateCommentsSidePanelVisibility(should_show_comments_action);
}

bool CommentsSidePanelCoordinator::ShouldShowCommentsAction(
    const tabs::TabInterface* tab) {
  if (!tab) {
    return false;
  }

  std::optional<tab_groups::TabGroupId> group = tab->GetGroup();
  if (!group.has_value()) {
    return false;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(group.value());

  // If the group is shared, we should show the comments action.
  return saved_group.has_value() && saved_group->is_shared_tab_group();
}

void CommentsSidePanelCoordinator::UpdateCommentsActionVisibility(
    bool should_show_comments_action) {
  PinnedToolbarActionsController* controller =
      browser_->GetFeatures().pinned_toolbar_actions_controller();
  if (!controller) {
    return;
  }

  if (should_show_comments_action ==
      controller->IsActionPoppedOut(kActionSidePanelShowComments)) {
    // Do nothing if the action is already in the correct state.
    return;
  }

  controller->ShowActionEphemerallyInToolbar(kActionSidePanelShowComments,
                                             should_show_comments_action);

  if (should_show_comments_action) {
    PinnedActionToolbarButton* button =
        controller->GetButtonFor(kActionSidePanelShowComments);
    CHECK(button);

    button->SetProperty(views::kElementIdentifierKey,
                        kSharedTabGroupCommentsActionElementId);
  }
}

void CommentsSidePanelCoordinator::UpdateCommentsSidePanelVisibility(
    bool should_show_comments_action) {
  SidePanelUI* const side_panel_ui = browser_->GetFeatures().side_panel_ui();

  SidePanelEntry::Key side_panel_entry_key(SidePanelEntry::Id::kComments);

  // TODO(crbug.com/430352059): This should also handle when a different side
  // panel is open.
  const bool side_panel_showing =
      side_panel_ui->IsSidePanelEntryShowing(side_panel_entry_key);

  if (should_show_comments_action == side_panel_showing) {
    // Do nothing if the side panel is in the correct state.
    return;
  }

  if (side_panel_showing) {
    SidePanelEntry* const side_panel_entry =
        SidePanelRegistry::From(browser_)->GetEntryForKey(side_panel_entry_key);
    // Close the side panel, setting the flag to recall the state when the
    // comments action is shown again.
    side_panel_ui->Close(side_panel_entry->type());
    side_panel_should_be_resumed_ = true;
    return;
  }

  if (side_panel_should_be_resumed_) {
    // Resume the side panel if it was closed due to changing the active tab.
    side_panel_ui->Show(SidePanelEntry::Key(SidePanelEntry::Id::kComments));
    side_panel_should_be_resumed_ = false;
  }
}

std::optional<std::u16string>
CommentsSidePanelCoordinator::GetSharedTabGroupName(
    const tabs::TabInterface* tab) {
  std::optional<tab_groups::TabGroupId> group = tab->GetGroup();
  if (!group.has_value()) {
    return std::nullopt;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(group.value());

  if (!saved_group.has_value() || !saved_group->is_shared_tab_group()) {
    return std::nullopt;
  }

  return saved_group->title();
}

void CommentsSidePanelCoordinator::UpdateSidePanelTitle(
    std::optional<std::u16string> group_name) {
  const bool has_group_name =
      group_name.has_value() && !group_name.value().empty();
  const std::u16string title =
      has_group_name
          ? l10n_util::GetStringFUTF16(
                IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE_WITH_NAME,
                group_name.value())
          : l10n_util::GetStringUTF16(
                IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE);

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionSidePanelShowComments, browser_->GetActions()->root_action_item());

  if (title != action_item->GetText()) {
    action_item->SetText(title);
  }
}

// static
bool CommentsSidePanelCoordinator::IsSupported() {
  return base::FeatureList::IsEnabled(
      collaboration::features::kCollaborationComments);
}

void CommentsSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kComments),
      base::BindRepeating(&CommentsSidePanelCoordinator::CreateCommentsWebView,
                          base::Unretained(this)),
      /*default_content_width_callback=*/base::NullCallback()));
}

std::unique_ptr<views::View>
CommentsSidePanelCoordinator::CreateCommentsWebView(
    SidePanelEntryScope& scope) {
  return std::make_unique<SidePanelWebUIViewT<CommentsSidePanelUI>>(
      scope, base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<WebUIContentsWrapperT<CommentsSidePanelUI>>(
          GURL(chrome::kChromeUICommentsSidePanelURL),
          scope.GetBrowserWindowInterface().GetProfile(),
          IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE,
          /*esc_closes_ui=*/false));
}
