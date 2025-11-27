// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"

#include <numeric>
#include <optional>
#include <unordered_set>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/collaboration/messaging/messaging_backend_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/data_sharing/collaboration_controller_delegate_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace tab_groups {

// static
bool SavedTabGroupUtils::IsEnabledForProfile(Profile* profile) {
  if (!profile) {
    return false;
  }

  return tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile) !=
         nullptr;
}

// static
void SavedTabGroupUtils::RemoveGroupFromTabstrip(
    const Browser* browser,
    const tab_groups::TabGroupId& local_group) {
  const Browser* const browser_with_local_group_id =
      browser ? browser
              : SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group);
  DCHECK(browser_with_local_group_id);
  if (!browser_with_local_group_id) {
    return;
  }

  TabStripModel* const tab_strip_model =
      browser_with_local_group_id->tab_strip_model();

  const int num_tabs_in_group =
      tab_strip_model->group_model()->GetTabGroup(local_group)->tab_count();
  if (tab_strip_model->count() == num_tabs_in_group) {
    // If the group about to be closed has all of the tabs in the browser, add a
    // new tab outside the group to prevent the browser from closing.
    tab_strip_model->delegate()->AddTabAt(GURL(), -1, true);
  }

  tab_strip_model->CloseAllTabsInGroup(local_group);
}

// static
void SavedTabGroupUtils::UngroupSavedGroup(Browser* browser,
                                           const base::Uuid& saved_group_guid) {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  if (!tab_group_service) {
    return;
  }

  // The group must exist and be in the tabstrip for ungrouping.
  const std::optional<SavedTabGroup> group =
      tab_group_service->GetGroup(saved_group_guid);
  if (!group.has_value() || !group->local_group_id().has_value()) {
    return;
  }

  base::OnceCallback<void()> ungroup_callback = base::BindOnce(
      [](const Browser* browser, const tab_groups::TabGroupId& local_group) {
        TabStripModel* const model = browser->tab_strip_model();
        const gfx::Range tab_range =
            model->group_model()->GetTabGroup(local_group)->ListTabs();

        std::vector<int> tabs;
        tabs.reserve(tab_range.length());
        for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
          tabs.push_back(i);
        }

        model->RemoveFromGroup(tabs);
      },
      browser, group->local_group_id().value());

  const bool closing_multiple_tabs = group->saved_tabs().size() > 1;
  DeletionDialogController::DialogMetadata dialog_metadata(
      DeletionDialogController::DialogType::UngroupSingle,
      /*closing_group_count=*/1, closing_multiple_tabs);
  browser->GetFeatures()
      .tab_group_deletion_dialog_controller()
      ->MaybeShowDialog(
          dialog_metadata,
          base::IgnoreArgs<DeletionDialogController::DeletionDialogTiming>(
              std::move(ungroup_callback)));
}

// static
void SavedTabGroupUtils::DeleteSavedGroup(Browser* browser,
                                          const base::Uuid& saved_group_guid) {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  if (!tab_group_service) {
    return;
  }

  const std::optional<SavedTabGroup> group =
      tab_group_service->GetGroup(saved_group_guid);
  if (!group.has_value()) {
    return;
  }

  if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
      group->is_shared_tab_group()) {
    // Shared groups must go through the CollaborationService to be removed
    // properly.
    collaboration::CollaborationService* collaboration_service =
        collaboration::CollaborationServiceFactory::GetForProfile(
            browser->profile());
    auto delegate = std::make_unique<CollaborationControllerDelegateDesktop>(
        const_cast<Browser*>(browser), data_sharing::FlowType::kDelete);
    collaboration_service->StartLeaveOrDeleteFlow(
        std::move(delegate), group->saved_guid(),
        collaboration::CollaborationServiceLeaveOrDeleteEntryPoint::kUnknown);
    return;
  }

  base::OnceCallback<void()> close_callback = base::BindOnce(
      [](const Browser* browser, const base::Uuid& saved_group_guid) {
        TabGroupSyncService* tab_group_service =
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(
                browser->profile());
        if (!tab_group_service) {
          return;
        }

        const std::optional<SavedTabGroup> group =
            tab_group_service->GetGroup(saved_group_guid);
        if (!group.has_value()) {
          return;
        }

        if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
            group->is_shared_tab_group()) {
          // Shared groups must go through the CollaborationService to be
          // removed properly.
          return;
        }

        std::optional<TabGroupId> local_group_id = group->local_group_id();

        if (local_group_id) {
          tab_group_service->RemoveGroup(local_group_id.value());
          SavedTabGroupUtils::RemoveGroupFromTabstrip(nullptr,
                                                      local_group_id.value());
        } else {
          tab_group_service->RemoveGroup(group->saved_guid());
        }
      },
      browser, saved_group_guid);

  DeletionDialogController::DialogMetadata saved_dialog_metadata(
      DeletionDialogController::DialogType::DeleteSingle,
      /*closing_group_count=*/1,
      /*closing_multiple_tabs=*/group->saved_tabs().size() > 1);

  DeletionDialogController::DialogMetadata shared_dialog_metadata(
      DeletionDialogController::DialogType::DeleteSingleShared,
      /*closing_group_count=*/1,
      /*closing_multiple_tabs=*/group->saved_tabs().size() > 1);
  shared_dialog_metadata.title_of_closing_group = group->title();

  const bool is_group_shared = group.value().collaboration_id().has_value();
  browser->GetFeatures()
      .tab_group_deletion_dialog_controller()
      ->MaybeShowDialog(
          is_group_shared ? shared_dialog_metadata : saved_dialog_metadata,
          base::IgnoreArgs<DeletionDialogController::DeletionDialogTiming>(
              std::move(close_callback)));
}

// static
void SavedTabGroupUtils::LeaveSharedGroup(const Browser* browser,
                                          const base::Uuid& saved_group_guid) {
  if (!tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
    return;
  }

  TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  if (!tab_group_service) {
    return;
  }

  const std::optional<SavedTabGroup> saved_group =
      tab_group_service->GetGroup(saved_group_guid);
  if (!saved_group) {
    return;
  }

  if (!saved_group->collaboration_id()) {
    return;
  }

  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser->profile());
  auto delegate = std::make_unique<CollaborationControllerDelegateDesktop>(
      const_cast<Browser*>(browser), data_sharing::FlowType::kLeave);
  collaboration_service->StartLeaveOrDeleteFlow(
      std::move(delegate), saved_group->saved_guid(),
      collaboration::CollaborationServiceLeaveOrDeleteEntryPoint::kUnknown);
}

// static
void SavedTabGroupUtils::MaybeShowSavedTabGroupDeletionDialog(
    Browser* browser,
    GroupDeletionReason reason,
    base::span<const TabGroupId> group_ids,
    base::OnceCallback<void(DeletionDialogController::DeletionDialogTiming)>
        callback) {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());

  CHECK(!group_ids.empty());

  // Confirmation is only needed if SavedTabGroups are being deleted. If the
  // service doesnt exist there are no saved tab groups.
  if (!tab_group_service) {
    std::move(callback).Run(
        DeletionDialogController::DeletionDialogTiming::Synchronous);
    return;
  }

  // If there's no way to show the group deletion dialog, then fallback to
  // running the callback.
  auto* const dialog_controller =
      browser->GetFeatures().tab_group_deletion_dialog_controller();
  if (!dialog_controller || !dialog_controller->CanShowDialog()) {
    std::move(callback).Run(
        DeletionDialogController::DeletionDialogTiming::Synchronous);
    return;
  }

  // Check to see if any of the groups are saved. If so then show the dialog,
  // else, just perform the callback. Also count the number of group and tabs.
  int num_saved_tabs = 0;
  int closing_group_count = 0;
  for (const auto& group : group_ids) {
    const std::optional<SavedTabGroup> saved_group =
        tab_group_service->GetGroup(group);
    if (!saved_group.has_value()) {
      continue;
    }

    num_saved_tabs += saved_group->saved_tabs().size();
    ++closing_group_count;
  }

  if (closing_group_count == 0) {
    std::move(callback).Run(
        DeletionDialogController::DeletionDialogTiming::Synchronous);
    return;
  }

  // TODO(tbergquist): If multiple types of groups are being closed, queue
  // multiple dialogs. For now, just act as if they are all the kind of the
  // first group.
  const auto saved_group_opt = tab_group_service->GetGroup(group_ids[0]);
  const tab_groups::SavedTabGroup& saved_group = saved_group_opt.value();

  DeletionDialogController::DialogType dialog_type =
      reason == GroupDeletionReason::ClosedLastTab
          ? DeletionDialogController::DialogType::CloseTabAndDelete
          : DeletionDialogController::DialogType::RemoveTabAndDelete;
  std::optional<base::OnceCallback<void()>> keep_callback = std::nullopt;

  if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
      saved_group.collaboration_id()) {
    if (reason == GroupDeletionReason::ClosedLastTab) {
      tab_group_service->OnLastTabClosed(saved_group);
    }
    collaboration::CollaborationService* collaboration_service =
        collaboration::CollaborationServiceFactory::GetForProfile(
            browser->profile());
    auto delegate = std::make_unique<CollaborationControllerDelegateDesktop>(
        const_cast<Browser*>(browser), data_sharing::FlowType::kClose);
    collaboration_service->StartLeaveOrDeleteFlow(
        std::move(delegate), saved_group.saved_guid(),
        collaboration::CollaborationServiceLeaveOrDeleteEntryPoint::kUnknown);
    return;
  }

  DeletionDialogController::DialogMetadata dialog_metadata(
      dialog_type, closing_group_count,
      /*closing_multiple_tabs=*/num_saved_tabs > 1);
  dialog_controller->MaybeShowDialog(dialog_metadata, std::move(callback),
                                     std::move(keep_callback));
}

// static
void SavedTabGroupUtils::OpenUrlInNewUngroupedTab(Browser* browser,
                                                  const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.started_from_context_menu = true;
  Navigate(&params);
}

// static
void SavedTabGroupUtils::OpenOrMoveSavedGroupToNewWindow(
    Browser* browser,
    const base::Uuid& saved_group_guid) {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  std::optional<SavedTabGroup> save_group =
      tab_group_service->GetGroup(saved_group_guid);
  // In case the group has been deleted or has no tabs.
  if (!save_group.has_value() || save_group->saved_tabs().empty()) {
    return;
  }

  const auto& local_group_id = save_group->local_group_id();
  Browser* const browser_with_local_group_id =
      local_group_id.has_value()
          ? SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id.value())
          : browser;

  if (!local_group_id.has_value()) {
    // Open the group in the browser the button was pressed.
    // NOTE: This action could cause `this` to be deleted. Make sure lines
    // following this have either copied data by value or hold pointers to the
    // objects it needs.
    tab_group_service->OpenTabGroup(
        saved_group_guid,
        std::make_unique<TabGroupActionContextDesktop>(
            browser_with_local_group_id, OpeningSource::kOpenedFromRevisitUi));
  }

  // Ensure that the saved group did open in the browser.
  save_group = tab_group_service->GetGroup(saved_group_guid);
  CHECK(save_group->local_group_id().has_value());

  // Move the open group to a new browser window.
  browser_with_local_group_id->tab_strip_model()
      ->delegate()
      ->MoveGroupToNewWindow(save_group->local_group_id().value());
}

// static
void SavedTabGroupUtils::ToggleGroupPinState(
    Browser* browser,
    const base::Uuid& saved_group_guid) {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  std::optional<SavedTabGroup> group =
      tab_group_service->GetGroup(saved_group_guid);
  CHECK(group.has_value());
  tab_group_service->UpdateGroupPosition(saved_group_guid, !group->is_pinned(),
                                         std::nullopt);
}

// static
SavedTabGroupTab SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
    content::WebContents* contents,
    base::Uuid saved_tab_group_id) {
  SavedTabGroupTab tab(
      contents->GetVisibleURL().is_empty() ? GURL(chrome::kChromeUINewTabURL)
                                           : contents->GetVisibleURL(),
      contents->GetTitle(), saved_tab_group_id, /*position=*/std::nullopt);
  return tab;
}

// static
content::NavigationHandle* SavedTabGroupUtils::OpenTabInBrowser(
    const GURL& url,
    Browser* browser,
    Profile* profile,
    WindowOpenDisposition disposition,
    std::optional<int> tabstrip_index,
    std::optional<tab_groups::TabGroupId> local_group_id) {
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = disposition;
  params.browser = browser;
  params.tabstrip_index = tabstrip_index.value_or(params.tabstrip_index);
  params.group = local_group_id;
  params.navigation_initiated_from_sync = true;
  params.window_action = NavigateParams::WindowAction::kNoAction;
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  return handle.get();
}

// static
Browser* SavedTabGroupUtils::GetBrowserWithTabGroupId(
    tab_groups::TabGroupId group_id) {
  Browser* result = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [group_id, &result](BrowserWindowInterface* browser) {
        const TabStripModel* const tab_strip_model =
            browser->GetTabStripModel();
        if (tab_strip_model && tab_strip_model->SupportsTabGroups() &&
            tab_strip_model->group_model()->ContainsTabGroup(group_id)) {
          result = browser->GetBrowserForMigrationOnly();
        }
        return !result;
      });
  return result;
}

// static
TabGroup* SavedTabGroupUtils::GetTabGroupWithId(
    tab_groups::TabGroupId group_id) {
  Browser* browser = GetBrowserWithTabGroupId(group_id);
  if (!browser) {
    return nullptr;
  }

  TabGroupModel* tab_group_model = browser->tab_strip_model()->group_model();
  CHECK(tab_group_model);

  return tab_group_model->GetTabGroup(group_id);
}

// static
std::vector<content::WebContents*> SavedTabGroupUtils::GetWebContentsesInGroup(
    tab_groups::TabGroupId group_id) {
  Browser* browser = GetBrowserWithTabGroupId(group_id);
  if (!browser) {
    return {};
  }

  const gfx::Range local_tab_group_indices =
      SavedTabGroupUtils::GetTabGroupWithId(group_id)->ListTabs();
  std::vector<content::WebContents*> contentses;
  for (size_t index = local_tab_group_indices.start();
       index < local_tab_group_indices.end(); index++) {
    contentses.push_back(browser->tab_strip_model()->GetWebContentsAt(index));
  }
  return contentses;
}

// static
std::vector<tabs::TabInterface*> SavedTabGroupUtils::GetTabsInGroup(
    tab_groups::TabGroupId group_id) {
  Browser* browser = GetBrowserWithTabGroupId(group_id);
  if (!browser) {
    return {};
  }

  const gfx::Range local_tab_group_indices =
      SavedTabGroupUtils::GetTabGroupWithId(group_id)->ListTabs();
  std::vector<tabs::TabInterface*> local_tabs;
  for (tabs::TabInterface* tab : browser->tab_strip_model()->GetTabsAtIndices(
           local_tab_group_indices.ToIntVector())) {
    local_tabs.push_back(tab);
  }
  return local_tabs;
}

// static
SavedTabGroup SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
    const tab_groups::LocalTabGroupID& local_id) {
  Browser* browser = GetBrowserWithTabGroupId(local_id);
  CHECK(browser);

  const TabGroup* local_group =
      browser->tab_strip_model()->group_model()->GetTabGroup(local_id);
  tab_groups::SavedTabGroup saved_tab_group(
      local_group->visual_data()->title(), local_group->visual_data()->color(),
      {}, std::nullopt, std::nullopt, local_id);
  saved_tab_group.SetPinned(
      tab_groups::SavedTabGroupUtils::ShouldAutoPinNewTabGroups(
          browser->profile()));

  const std::vector<content::WebContents*>& web_contentses =
      tab_groups::SavedTabGroupUtils::GetWebContentsesInGroup(local_id);
  for (content::WebContents* web_contents : web_contentses) {
    tab_groups::SavedTabGroupTab saved_tab_group_tab =
        tab_groups::SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
            web_contents, saved_tab_group.saved_guid());
    saved_tab_group.AddTabLocally(std::move(saved_tab_group_tab));
  }

  return saved_tab_group;
}

// static
std::unordered_set<std::string> SavedTabGroupUtils::GetURLsInSavedTabGroup(
    Profile* profile,
    const base::Uuid& saved_id) {
  TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);

  const std::optional<SavedTabGroup> saved_group =
      tab_group_service->GetGroup(saved_id);
  CHECK(saved_group.has_value());

  std::unordered_set<std::string> saved_urls;
  for (const tab_groups::SavedTabGroupTab& saved_tab :
       saved_group->saved_tabs()) {
    saved_urls.emplace(saved_tab.url().spec());
  }

  return saved_urls;
}

// static
void SavedTabGroupUtils::FocusFirstTabOrWindowInOpenGroup(
    tab_groups::TabGroupId local_group_id) {
  Browser* browser_for_activation =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id);

  // Only activate the tab group's first tab, if it exists in any browser's
  // tabstrip model and it is not in the active tab in the tab group.
  CHECK(browser_for_activation);
  TabGroup* tab_group =
      browser_for_activation->tab_strip_model()->group_model()->GetTabGroup(
          local_group_id);

  gfx::Range tab_group_index_range = tab_group->ListTabs();
  CHECK(!tab_group_index_range.is_empty());

  int active_index = browser_for_activation->tab_strip_model()->active_index();
  CHECK_GE(active_index, 0);

  if (active_index >= static_cast<int>(tab_group_index_range.GetMin()) &&
      active_index < static_cast<int>(tab_group_index_range.GetMax())) {
    browser_for_activation->window()->Activate();
    return;
  }

  browser_for_activation->ActivateContents(
      tab_group->GetFirstTab()->GetContents());

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Focused"));
}

// static
ui::TrackedElement* SavedTabGroupUtils::GetAnchorElementForTabGroupsV2IPH(
    const ui::ElementTracker::ElementList& elements) {
  // If there's no AppMenu with an element ID, we cant find a
  // browser to show the IPH on.
  if (elements.empty()) {
    return nullptr;
  }

  // Get the context from the first element. This is the browser
  // that the IPH will be displayed in.
  ui::ElementContext context = elements[0]->context();

  // Get the OverflowButton from the bookmarks bar. If it exists
  // use it as the anchor.
  ui::TrackedElement* overflow_button_element =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kSavedTabGroupOverflowButtonElementId, context);
  if (overflow_button_element) {
    return overflow_button_element;
  }

  // Fallback to the AppMenuButton.
  return elements[0];
}

// static
user_education::HelpBubbleArrow SavedTabGroupUtils::GetArrowForTabGroupsV2IPH(
    const ui::TrackedElement* el) {
  // Because the overflow button is often on the left side of the browser
  // window, switch the arrow to the top left to avoid bubble placement that
  // would otherwise leave the browser's bounds.
  return el->identifier() == kSavedTabGroupOverflowButtonElementId
             ? user_education::HelpBubbleArrow::kTopLeft
             : user_education::HelpBubbleArrow::kTopRight;
}

// static
bool SavedTabGroupUtils::ShouldAutoPinNewTabGroups(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      tab_groups::prefs::kAutoPinNewTabGroups);
}

// static
bool SavedTabGroupUtils::AreSavedTabGroupsSyncedForProfile(Profile* profile) {
  const syncer::SyncService* const sync_service =
      SyncServiceFactory::GetForProfile(profile);

  return sync_service &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kSavedTabGroups);
}

// static
bool SavedTabGroupUtils::SupportsSharedTabGroups() {
  return data_sharing::features::IsDataSharingFunctionalityEnabled();
}

// static
bool SavedTabGroupUtils::IsOwnerOfSharedTabGroup(Profile* profile,
                                                 const base::Uuid& sync_id) {
  // TODO(380515575): Create a function to determine if the user is signed in or
  // not instead of checking here.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  if (account.IsEmpty()) {
    return true;
  }

  TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  if (!tab_group_service) {
    return true;
  }

  const std::optional<SavedTabGroup> saved_group =
      tab_group_service->GetGroup(sync_id);
  if (!saved_group) {
    return true;
  }

  std::optional<syncer::CollaborationId> collaboration_id =
      saved_group->collaboration_id();
  if (!collaboration_id) {
    return true;
  }

  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  if (!collaboration_service) {
    return true;
  }

  data_sharing::MemberRole member_role =
      collaboration_service->GetCurrentUserRoleForGroup(
          data_sharing::GroupId(collaboration_id.value().value()));

  return data_sharing::MemberRole::kOwner == member_role;
}

// static
std::vector<data_sharing::GroupMember>
SavedTabGroupUtils::GetMembersOfSharedTabGroup(
    Profile* profile,
    const syncer::CollaborationId& collaboration_id) {
  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  if (!collaboration_service) {
    return {};
  }

  const std::optional<data_sharing::GroupData> group_data =
      collaboration_service->GetGroupData(
          data_sharing::GroupId(collaboration_id.value()));

  if (!group_data.has_value()) {
    return {};
  }

  return group_data->members;
}

// static
std::optional<data_sharing::GroupId> SavedTabGroupUtils::GetDataSharingGroupId(
    Profile* profile,
    LocalTabGroupID group_id) {
  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  CHECK(tab_group_sync_service);

  std::optional<SavedTabGroup> saved_tab_group =
      tab_group_sync_service->GetGroup(group_id);
  if (!saved_tab_group) {
    return std::nullopt;
  }

  std::optional<syncer::CollaborationId> collaboration_id =
      saved_tab_group->collaboration_id();
  if (!collaboration_id.has_value()) {
    return std::nullopt;
  }

  return data_sharing::GroupId(collaboration_id.value().value());
}

// static
std::vector<collaboration::messaging::ActivityLogItem>
SavedTabGroupUtils::GetRecentActivity(Profile* profile,
                                      LocalTabGroupID group_id,
                                      std::optional<LocalTabID> tab_id) {
  auto* messaging_service =
      collaboration::messaging::MessagingBackendServiceFactory::GetForProfile(
          profile);
  CHECK(messaging_service);

  std::optional<data_sharing::GroupId> collaboration_group_id =
      SavedTabGroupUtils::GetDataSharingGroupId(profile, group_id);
  if (!collaboration_group_id.has_value()) {
    return {};
  }

  collaboration::messaging::ActivityLogQueryParams activity_log_params;
  activity_log_params.result_length =
      tab_id.has_value() ? 1 : RecentActivityBubbleDialogView::kMaxNumberRows;
  activity_log_params.collaboration_id = collaboration_group_id.value();
  activity_log_params.local_tab_id = tab_id;

  return messaging_service->GetActivityLog(activity_log_params);
}

// static
tabs::TabInterface* SavedTabGroupUtils::GetGroupedTab(LocalTabGroupID group_id,
                                                      LocalTabID tab_id) {
  const Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
  if (!browser) {
    return nullptr;
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  const gfx::Range tab_indices =
      tab_strip_model->group_model()->GetTabGroup(group_id)->ListTabs();
  for (tabs::TabInterface* tab : browser->tab_strip_model()->GetTabsAtIndices(
           tab_indices.ToIntVector())) {
    if (tab->GetHandle().raw_value() == tab_id) {
      return tab;
    }
  }

  return nullptr;
}

void SavedTabGroupUtils::PerformTabGroupMenuAction(
    const TabGroupMenuAction& action,
    const TabGroupMenuContext& context,
    Browser* browser,
    TabGroupSyncService* tab_group_service) {
  auto type = action.type;
  if (type == TabGroupMenuAction::Type::OPEN_URL) {
    SavedTabGroupUtils::OpenUrlInNewUngroupedTab(
        browser, std::get<GURL>(action.element));
    return;
  }

  auto uuid = std::get<base::Uuid>(action.element);
  switch (type) {
    case TabGroupMenuAction::Type::OPEN_IN_BROWSER: {
      base::RecordAction(base::UserMetricsAction(
          "TabGroups_SavedTabGroups_TabGroupSubmenu_Opened"));

      if (context == TabGroupMenuContext::APP_MENU) {
        base::RecordAction(base::UserMetricsAction(
            "TabGroups_SavedTabGroups_OpenedFromTabGroupsAppMenu"));
      }

      bool will_open_shared_group = false;
      if (std::optional<tab_groups::SavedTabGroup> saved_group =
              tab_group_service->GetGroup(uuid)) {
        will_open_shared_group = !saved_group->local_group_id().has_value() &&
                                 saved_group->is_shared_tab_group();
      }

      tab_group_service->OpenTabGroup(
          uuid, std::make_unique<TabGroupActionContextDesktop>(
                    browser, OpeningSource::kOpenedFromRevisitUi));

      if (will_open_shared_group) {
        saved_tab_groups::metrics::RecordSharedTabGroupRecallType(
            saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::
                kOpenedFromSubmenu);
      }
      break;
    }
    case TabGroupMenuAction::Type::OPEN_OR_MOVE_TO_NEW_WINDOW:
      base::RecordAction(base::UserMetricsAction(
          "TabGroups_SavedTabGroups_MoveGroupToNewWindow"));
      SavedTabGroupUtils::OpenOrMoveSavedGroupToNewWindow(browser, uuid);
      break;
    case TabGroupMenuAction::Type::PIN_OR_UNPIN_GROUP:

      if (std::optional<tab_groups::SavedTabGroup> group =
              tab_group_service->GetGroup(uuid)) {
        if (group->is_pinned()) {
          base::RecordAction(
              base::UserMetricsAction("TabGroups_SavedTabGroups_Unpinned"));
        } else {
          base::RecordAction(
              base::UserMetricsAction("TabGroups_SavedTabGroups_Pinned"));
        }
      }

      SavedTabGroupUtils::ToggleGroupPinState(browser, uuid);
      break;
    case TabGroupMenuAction::Type::DELETE_GROUP:
      SavedTabGroupUtils::DeleteSavedGroup(browser, uuid);
      break;
    case TabGroupMenuAction::Type::LEAVE_GROUP:
      SavedTabGroupUtils::LeaveSharedGroup(browser, uuid);
      break;
    case TabGroupMenuAction::Type::OPEN_URL:
    case TabGroupMenuAction::Type::DEFAULT:
      break;
  }
}

}  // namespace tab_groups
