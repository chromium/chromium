// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"

#include <numeric>
#include <unordered_set>

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace tab_groups {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SavedTabGroupUtils, kDeleteGroupMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SavedTabGroupUtils,
                                      kMoveGroupToNewWindowMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SavedTabGroupUtils,
                                      kToggleGroupPinStateMenuItem);

void SavedTabGroupUtils::OpenUrlToBrowser(Browser* browser,
                                          const GURL& url,
                                          int event_flags) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.started_from_context_menu = true;
  Navigate(&params);
}

void SavedTabGroupUtils::OpenOrMoveSavedGroupToNewWindow(
    Browser* browser,
    const SavedTabGroup* save_group,
    int event_flags) {
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
    auto* service =
        SavedTabGroupServiceFactory::GetForProfile(browser->profile());
    service->OpenSavedTabGroupInBrowser(browser_with_local_group_id,
                                        save_group->saved_guid());
  }

  // Move the open group to a new browser window.
  browser_with_local_group_id->tab_strip_model()
      ->delegate()
      ->MoveGroupToNewWindow(save_group->local_group_id().value());
}

void SavedTabGroupUtils::DeleteSavedTabGroup(Browser* browser,
                                             const SavedTabGroup* saved_group,
                                             int event_flags) {
  const auto& local_group_id = saved_group->local_group_id();
  auto* const service =
      SavedTabGroupServiceFactory::GetForProfile(browser->profile());

  if (local_group_id.has_value()) {
    const Browser* const browser_with_local_group_id =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id.value());

    // Keep the opened tab group in the tabstrip but remove the SavedTabGroup
    // data from the model.
    TabGroup* const tab_group = browser_with_local_group_id->tab_strip_model()
                                    ->group_model()
                                    ->GetTabGroup(local_group_id.value());

    service->UnsaveGroup(local_group_id.value());

    // Notify observers to update the tab group header.
    // TODO(dljames): Find a way to move this into
    // SavedTabGroupKeyedService::DisconnectLocalTabGroup. The goal is to
    // abstract this logic from the button in case we need to do similar
    // functionality elsewhere in the future. Ensure this change works when
    // dragging a Saved group out of the window.
    tab_group->SetVisualData(*tab_group->visual_data());
  } else {
    // Remove the SavedTabGroup from the model. No need to worry about updating
    // tabstrip, since this group is not open.
    service->model()->Remove(saved_group->saved_guid());
  }
}

void SavedTabGroupUtils::ToggleGroupPinState(Browser* browser,
                                             base::Uuid id,
                                             int event_flags) {
  auto* const service =
      SavedTabGroupServiceFactory::GetForProfile(browser->profile());
  service->model()->TogglePinState(id);
}

std::unique_ptr<ui::DialogModel>
SavedTabGroupUtils::CreateSavedTabGroupContextMenuModel(
    Browser* browser,
    const base::Uuid& saved_guid,
    bool show_pin_unpin_option) {
  const auto* const service =
      SavedTabGroupServiceFactory::GetForProfile(browser->profile());
  const auto* const saved_group = service->model()->Get(saved_guid);
  const auto& local_group_id = saved_group->local_group_id();

  ui::DialogModel::Builder dialog_model = ui::DialogModel::Builder();

  const std::u16string move_or_open_group_text =
      local_group_id.has_value()
          ? l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW)
          : l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW);

  bool should_enable_move_menu_item = true;
  if (local_group_id.has_value()) {
    const Browser* const browser_with_local_group_id =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id.value());
    const TabStripModel* const tab_strip_model =
        browser_with_local_group_id->tab_strip_model();

    // Show the menu item if there are tabs outside of the saved group.
    should_enable_move_menu_item =
        tab_strip_model->count() != tab_strip_model->group_model()
                                        ->GetTabGroup(local_group_id.value())
                                        ->tab_count();
  }

  dialog_model.AddMenuItem(
      ui::ImageModel::FromVectorIcon(kMoveGroupToNewWindowRefreshIcon),
      move_or_open_group_text,
      base::BindRepeating(&SavedTabGroupUtils::OpenOrMoveSavedGroupToNewWindow,
                          browser, saved_group),
      ui::DialogModelMenuItem::Params()
          .SetId(kMoveGroupToNewWindowMenuItem)
          .SetIsEnabled(should_enable_move_menu_item));

  if (tab_groups::IsTabGroupsSaveUIUpdateEnabled() && show_pin_unpin_option) {
    dialog_model.AddMenuItem(
        ui::ImageModel::FromVectorIcon(saved_group->is_pinned()
                                           ? kKeepPinFilledChromeRefreshIcon
                                           : kKeepPinChromeRefreshIcon),
        l10n_util::GetStringUTF16(saved_group->is_pinned()
                                      ? IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP
                                      : IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP),
        base::BindRepeating(&SavedTabGroupUtils::ToggleGroupPinState, browser,
                            saved_group->saved_guid()),
        ui::DialogModelMenuItem::Params().SetId(kToggleGroupPinStateMenuItem));
  }

  dialog_model
      .AddMenuItem(
          ui::ImageModel::FromVectorIcon(kCloseGroupRefreshIcon),
          l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP),
          base::BindRepeating(&SavedTabGroupUtils::DeleteSavedTabGroup, browser,
                              saved_group),
          ui::DialogModelMenuItem::Params().SetId(kDeleteGroupMenuItem))
      .AddSeparator();

  for (const SavedTabGroupTab& tab : saved_group->saved_tabs()) {
    const ui::ImageModel& image =
        tab.favicon().has_value()
            ? ui::ImageModel::FromImage(tab.favicon().value())
            : favicon::GetDefaultFaviconModel(
                  GetTabGroupBookmarkColorId(saved_group->color()));
    const std::u16string title =
        tab.title().empty() ? base::UTF8ToUTF16(tab.url().spec()) : tab.title();
    dialog_model.AddMenuItem(
        image, title,
        base::BindRepeating(&SavedTabGroupUtils::OpenUrlToBrowser, browser,
                            tab.url()));
  }

  return dialog_model.Build();
}

SavedTabGroupTab SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
    content::WebContents* contents,
    base::Uuid saved_tab_group_id) {
  // in order to protect from filesystem access or chrome settings page use,
  // replace the URL with the new tab page, when creating from sync or an
  // unsaved group.
  if (!IsURLValidForSavedTabGroups(contents->GetVisibleURL())) {
    return SavedTabGroupTab(GURL(chrome::kChromeUINewTabURL), u"Unsavable tab",
                            saved_tab_group_id,
                            /*position=*/std::nullopt);
  }

  SavedTabGroupTab tab(contents->GetVisibleURL(), contents->GetTitle(),
                       saved_tab_group_id, /*position=*/std::nullopt);
  tab.SetFavicon(favicon::TabFaviconFromWebContents(contents));
  return tab;
}

content::WebContents* SavedTabGroupUtils::OpenTabInBrowser(
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
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  return handle ? handle->GetWebContents() : nullptr;
}

// static
Browser* SavedTabGroupUtils::GetBrowserWithTabGroupId(
    tab_groups::TabGroupId group_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    const TabStripModel* const tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model && tab_strip_model->SupportsTabGroups() &&
        tab_strip_model->group_model()->ContainsTabGroup(group_id)) {
      return browser;
    }
  }
  return nullptr;
}

// static
TabGroup* SavedTabGroupUtils::GetTabGroupWithId(
    tab_groups::TabGroupId group_id) {
  Browser* browser = GetBrowserWithTabGroupId(group_id);
  if (!browser || !browser->tab_strip_model() ||
      !browser->tab_strip_model()->SupportsTabGroups()) {
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
  if (!browser || !browser->tab_strip_model() ||
      !browser->tab_strip_model()->SupportsTabGroups()) {
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

std::unordered_set<std::string> SavedTabGroupUtils::GetURLsInSavedTabGroup(
    const tab_groups::SavedTabGroupKeyedService& saved_tab_group_service,
    const base::Uuid& saved_id) {
  const tab_groups::SavedTabGroup* const saved_group =
      saved_tab_group_service.model()->Get(saved_id);
  CHECK(saved_group);

  std::unordered_set<std::string> saved_urls;
  for (const tab_groups::SavedTabGroupTab& saved_tab :
       saved_group->saved_tabs()) {
    saved_urls.emplace(saved_tab.url().spec());
  }

  return saved_urls;
}

void SavedTabGroupUtils::MoveGroupToExistingWindow(
    Browser* source_browser,
    Browser* target_browser,
    const tab_groups::TabGroupId& local_group_id,
    const base::Uuid& saved_group_id) {
  CHECK(source_browser);
  CHECK(target_browser);
  tab_groups::SavedTabGroupKeyedService* const service =
      SavedTabGroupServiceFactory::GetForProfile(source_browser->profile());
  CHECK(service);

  // Find the grouped tabs in `source_browser`.
  gfx::Range tabs_to_move = source_browser->tab_strip_model()
                                ->group_model()
                                ->GetTabGroup(local_group_id)
                                ->ListTabs();
  int num_tabs_to_move = tabs_to_move.length();

  std::vector<int> tab_indicies_to_move(num_tabs_to_move);
  std::iota(tab_indicies_to_move.begin(), tab_indicies_to_move.end(),
            tabs_to_move.start());

  // Disconnect the group and move the tabs to `target_browser`.
  service->DisconnectLocalTabGroup(local_group_id);
  chrome::MoveTabsToExistingWindow(source_browser, target_browser,
                                   tab_indicies_to_move);

  // Tabs should be in `target_browser` now. Regroup them.
  int total_tabs = target_browser->tab_strip_model()->count();
  int first_tab_moved = total_tabs - num_tabs_to_move;
  std::vector<int> tabs_to_add_to_group(num_tabs_to_move);
  std::iota(tabs_to_add_to_group.begin(), tabs_to_add_to_group.end(),
            first_tab_moved);

  // Add group the tabs using the same local id, and reconnect everything.
  target_browser->tab_strip_model()->AddToGroupForRestore(tabs_to_add_to_group,
                                                          local_group_id);
  service->ConnectLocalTabGroup(local_group_id, saved_group_id);
}

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

  std::optional<int> first_tab = tab_group->GetFirstTab();
  std::optional<int> last_tab = tab_group->GetLastTab();
  int active_index = browser_for_activation->tab_strip_model()->active_index();
  CHECK(first_tab.has_value());
  CHECK(last_tab.has_value());
  CHECK_GE(active_index, 0);

  if (active_index >= first_tab.value() && active_index <= last_tab) {
    browser_for_activation->window()->Activate();
    return;
  }

  browser_for_activation->ActivateContents(
      browser_for_activation->tab_strip_model()->GetWebContentsAt(
          first_tab.value()));

  base::RecordAction(
      base::UserMetricsAction("TabGroups_SavedTabGroups_Focused"));
}

// static
bool SavedTabGroupUtils::IsURLValidForSavedTabGroups(const GURL& gurl) {
  return gurl.SchemeIsHTTPOrHTTPS() || gurl == GURL(chrome::kChromeUINewTabURL);
}

// static
void SavedTabGroupUtils::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kTabGroupSavesUIUpdateMigrated, false);
}

// static
bool SavedTabGroupUtils::IsTabGroupSavesUIUpdateMigrated(
    PrefService* pref_service) {
  return pref_service->GetBoolean(prefs::kTabGroupSavesUIUpdateMigrated);
}

// static
void SavedTabGroupUtils::SetTabGroupSavesUIUpdateMigrated(
    PrefService* pref_service) {
  pref_service->SetBoolean(prefs::kTabGroupSavesUIUpdateMigrated, true);
}

}  // namespace tab_groups
