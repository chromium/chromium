// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/tab_group_dynamic_menu.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tabs/public/tab_group.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/favicon_size.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(base::Uuid*)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(base::Uuid, kSavedTabGroupGuidKey)

TabGroupDynamicMenu::TabGroupDynamicMenu(BrowserWindowInterface* bwi)
    : browser_window_interface_(bwi) {}

TabGroupDynamicMenu::~TabGroupDynamicMenu() = default;

void TabGroupDynamicMenu::BuildTabGroupsAction(
    actions::BaseAction* parent_item) {
  if (!parent_item || !browser_window_interface_) {
    return;
  }

  cancelable_task_tracker_.TryCancelAll();
  Profile* profile = browser_window_interface_->GetProfile();

  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);

  std::vector<base::Uuid> group_ids =
      tab_groups::TabGroupMenuUtils::GetGroupsForDisplaySortedByCreationTime(
          tab_group_service);

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  for (const base::Uuid& uuid : group_ids) {
    const std::optional<tab_groups::SavedTabGroup> group =
        tab_group_service->GetGroup(uuid);

    std::u16string group_title =
        tab_groups::TabGroupMenuUtils::GetMenuTextForGroup(*group);
    ui::ImageModel group_icon = ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? kCircleFilledIcon
                                          : kTabGroupOldIcon,
        GetTabGroupContextMenuColorId(group->color()), gfx::kFaviconSize);

    auto group_builder = actions::ActionItem::Builder();
    group_builder.SetText(group_title).SetImage(group_icon);
    auto group_action = std::move(group_builder).Build();

    BuildTabGroupCommands(group, group_action.get(), uuid, profile);
    BuildTabGroupData(group, favicon_service, group_action.get());

    parent_item->AddChild(std::move(group_action));
  }
}

void TabGroupDynamicMenu::OnFaviconDataAvailable(
    base::WeakPtr<actions::ActionItem> action_item,
    const favicon_base::FaviconImageResult& image_result) {
  if (action_item && !image_result.image.IsEmpty()) {
    action_item->SetImage(ui::ImageModel::FromImage(image_result.image));
  }
}

void TabGroupDynamicMenu::BuildTabGroupCommands(
    std::optional<tab_groups::SavedTabGroup> group,
    actions::BaseAction* parent_item,
    const base::Uuid& uuid,
    Profile* profile) {
  auto open_in_browser_item =
      actions::ActionItem::Builder(
          base::BindRepeating(
              &TabGroupDynamicMenu::PerformTabGroupAction,
              base::Unretained(this),
              tab_groups::TabGroupMenuAction::Type::OPEN_IN_BROWSER,
              browser_window_interface_))
          .SetActionId(kActionTabGroupOpenInBrowser)
          .SetText(l10n_util::GetStringUTF16(IDS_OPEN_GROUP_IN_BROWSER_MENU))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kOpenInBrowserIcon
                                                : kOpenInBrowserOldIcon))
          .SetProperty(ActionAppMenuManager::kDisplayTypeKey,
                       ActionAppMenuManager::DisplayType::kRow)
          .Build();

  open_in_browser_item->SetProperty(kSavedTabGroupGuidKey,
                                    std::make_unique<base::Uuid>(uuid));
  open_in_browser_item->SetEnabled(!group->local_group_id().has_value());
  parent_item->AddChild(std::move(open_in_browser_item));

  bool is_local = group->local_group_id().has_value();
  std::optional<std::u16string> move_text_override =
      is_local ? std::make_optional(l10n_util::GetStringUTF16(
                     IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW))
               : std::nullopt;

  bool should_enable_move = true;
  if (is_local) {
    const BrowserWindowInterface* const browser_with_local_group =
        tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
            group->local_group_id().value());
    if (browser_with_local_group) {
      const TabStripModel* const tsm =
          browser_with_local_group->GetTabStripModel();
      if (tsm && tsm->group_model() &&
          tsm->group_model()->GetTabGroup(group->local_group_id().value())) {
        should_enable_move =
            tsm->count() != tsm->group_model()
                                ->GetTabGroup(group->local_group_id().value())
                                ->tab_count();
      }
    }
  }

  auto move_or_open_item =
      actions::ActionItem::Builder(
          base::BindRepeating(
              &TabGroupDynamicMenu::PerformTabGroupAction,
              base::Unretained(this),
              tab_groups::TabGroupMenuAction::Type::OPEN_OR_MOVE_TO_NEW_WINDOW,
              browser_window_interface_))
          .SetActionId(kActionTabGroupOpenInNewWindow)
          .SetText(l10n_util::GetStringUTF16(
              IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kMoveGroupIcon
                  : kMoveGroupToNewWindowRefreshOldIcon))
          .SetProperty(ActionAppMenuManager::kDisplayTypeKey,
                       ActionAppMenuManager::DisplayType::kRow)
          .Build();

  move_or_open_item->SetProperty(kSavedTabGroupGuidKey,
                                 std::make_unique<base::Uuid>(uuid));
  if (move_text_override.has_value()) {
    move_or_open_item->SetProperty(
        ActionAppMenuManager::kTextOverrideKey,
        std::make_unique<std::u16string>(move_text_override.value()));
  }

  move_or_open_item->SetEnabled(should_enable_move);
  parent_item->AddChild(std::move(move_or_open_item));

  bool group_pinned = group->is_pinned();
  std::optional<std::u16string> pin_text_override =
      group_pinned ? std::make_optional(l10n_util::GetStringUTF16(
                         IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP))
                   : std::nullopt;
  std::optional<ui::ImageModel> pin_icon_override =
      group_pinned ? std::make_optional(ui::ImageModel::FromVectorIcon(
                         features::IsRoundedIconsEnabled() ? kKeepOffIcon
                                                           : kKeepOffOldIcon))
                   : std::nullopt;

  auto pin_item =
      actions::ActionItem::Builder(
          base::BindRepeating(
              &TabGroupDynamicMenu::PerformTabGroupAction,
              base::Unretained(this),
              tab_groups::TabGroupMenuAction::Type::PIN_OR_UNPIN_GROUP,
              browser_window_interface_))
          .SetActionId(kActionTabGroupPin)
          .SetText(
              l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kKeepIcon : kKeepOldIcon))
          .SetProperty(ActionAppMenuManager::kDisplayTypeKey,
                       ActionAppMenuManager::DisplayType::kRow)
          .Build();

  pin_item->SetProperty(kSavedTabGroupGuidKey,
                        std::make_unique<base::Uuid>(uuid));
  if (pin_text_override.has_value()) {
    pin_item->SetProperty(
        ActionAppMenuManager::kTextOverrideKey,
        std::make_unique<std::u16string>(pin_text_override.value()));
  }
  if (pin_icon_override.has_value()) {
    pin_item->SetProperty(
        ActionAppMenuManager::kIconOverrideKey,
        std::make_unique<ui::ImageModel>(pin_icon_override.value()));
  }

  parent_item->AddChild(std::move(pin_item));

  bool is_owner =
      tab_groups::SavedTabGroupUtils::IsOwnerOfSharedTabGroup(profile, uuid);
  std::optional<std::u16string> delete_text_override =
      !is_owner ? std::make_optional(
                      l10n_util::GetStringUTF16(IDS_DATA_SHARING_LEAVE_GROUP))
                : std::nullopt;

  auto delete_or_leave_item =
      actions::ActionItem::Builder(
          base::BindRepeating(
              &TabGroupDynamicMenu::PerformTabGroupAction,
              base::Unretained(this),
              tab_groups::TabGroupMenuAction::Type::DELETE_GROUP,
              browser_window_interface_))
          .SetActionId(kActionTabGroupDelete)
          .SetText(l10n_util::GetStringUTF16(
              IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kTabCloseIcon
                                                : kCloseGroupRefreshOldIcon))
          .SetProperty(ActionAppMenuManager::kDisplayTypeKey,
                       ActionAppMenuManager::DisplayType::kRow)
          .Build();

  delete_or_leave_item->SetProperty(kSavedTabGroupGuidKey,
                                    std::make_unique<base::Uuid>(uuid));
  if (delete_text_override.has_value()) {
    delete_or_leave_item->SetProperty(
        ActionAppMenuManager::kTextOverrideKey,
        std::make_unique<std::u16string>(delete_text_override.value()));
  }

  parent_item->AddChild(std::move(delete_or_leave_item));
  parent_item->AddChild(ActionAppMenuManager::CreateDividerActionItem());
}

void TabGroupDynamicMenu::BuildTabGroupData(
    std::optional<tab_groups::SavedTabGroup> group,
    favicon::FaviconService* favicon_service,
    actions::ActionItem* parent_item) {
  auto header_item = ActionAppMenuManager::CreateSectionHeaderActionItem(
      l10n_util::GetStringUTF16(IDS_TABS_TITLE_CXMENU));
  parent_item->AddChild(std::move(header_item));

  for (const tab_groups::SavedTabGroupTab& tab : group->saved_tabs()) {
    std::u16string tab_title =
        tab_groups::TabGroupMenuUtils::GetMenuTextForTab(tab);
    ui::ImageModel tab_icon = favicon::GetDefaultFaviconModel(
        GetTabGroupBookmarkColorId(group->color()));
    GURL tab_url = tab.url();

    auto tab_builder = actions::ActionItem::Builder();
    tab_builder.SetText(tab_title).SetImage(tab_icon).SetInvokeActionCallback(
        base::BindRepeating(
            [](base::WeakPtr<TabGroupDynamicMenu> self, GURL url,
               actions::ActionItem* item,
               actions::ActionInvocationContext context) {
              if (!self || !self->browser_window_interface_) {
                return;
              }
              tab_groups::TabGroupMenuAction action(
                  tab_groups::TabGroupMenuAction::Type::OPEN_URL, url);
              tab_groups::SavedTabGroupUtils::PerformTabGroupMenuAction(
                  action, tab_groups::TabGroupMenuContext::APP_MENU,
                  self->browser_window_interface_, nullptr);
            },
            GetWeakPtr(), tab_url));

    auto built_tab_action = std::move(tab_builder).Build();
    favicon_service->GetFaviconImageForPageURL(
        tab_url,
        base::BindOnce(&TabGroupDynamicMenu::OnFaviconDataAvailable,
                       weak_ptr_factory_.GetWeakPtr(),
                       built_tab_action->GetAsWeakPtr()),
        &cancelable_task_tracker_);

    parent_item->AddChild(std::move(built_tab_action));
  }
}

void TabGroupDynamicMenu::PerformTabGroupAction(
    tab_groups::TabGroupMenuAction::Type type,
    BrowserWindowInterface* bwi,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  if (!bwi || !item) {
    return;
  }
  base::Uuid* guid = item->GetProperty(kSavedTabGroupGuidKey);
  if (!guid || !guid->is_valid()) {
    return;
  }

  tab_groups::TabGroupMenuAction::Type final_type = type;

  // Find it we are the owner of the group we want to delete, if not we change
  // type to leave
  if (type == tab_groups::TabGroupMenuAction::Type::DELETE_GROUP) {
    bool is_owner = tab_groups::SavedTabGroupUtils::IsOwnerOfSharedTabGroup(
        bwi->GetProfile(), *guid);
    if (!is_owner) {
      final_type = tab_groups::TabGroupMenuAction::Type::LEAVE_GROUP;
    }
  }

  tab_groups::TabGroupMenuAction action(final_type, *guid);
  tab_groups::TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(bwi->GetProfile());
  tab_groups::SavedTabGroupUtils::PerformTabGroupMenuAction(
      action, tab_groups::TabGroupMenuContext::APP_MENU, bwi, service);
}
