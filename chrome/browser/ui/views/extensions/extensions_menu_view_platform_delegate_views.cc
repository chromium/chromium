// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view_platform_delegate_views.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extension_action_platform_delegate_views.h"
#include "chrome/browser/ui/views/extensions/extension_view_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_id.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

// Returns the state for the main page in the menu.
enum class MainPageState {
  // Site is restricted to all extensions.
  kRestrictedSite,
  // Site is restricted all non-enterprise extensions by policy.
  kPolicyBlockedSite,
  // User blocked all extensions access to the site.
  kUserBlockedSite,
  // User can customize each extension's access to the site.
  kUserCustomizedSite,
};

MainPageState GetMainPageState(Profile& profile,
                               const ToolbarActionsModel& toolbar_model,
                               content::WebContents& web_contents) {
  const GURL& url = web_contents.GetLastCommittedURL();
  if (toolbar_model.IsRestrictedUrl(url)) {
    return MainPageState::kRestrictedSite;
  }

  if (toolbar_model.IsPolicyBlockedHost(url)) {
    return MainPageState::kPolicyBlockedSite;
  }

  PermissionsManager::UserSiteSetting site_setting =
      PermissionsManager::Get(&profile)->GetUserSiteSetting(
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin());
  if (site_setting ==
      PermissionsManager::UserSiteSetting::kBlockAllExtensions) {
    return MainPageState::kUserBlockedSite;
  }

  return MainPageState::kUserCustomizedSite;
}

// Returns the extension for `extension_id`.
const extensions::Extension* GetExtension(
    Browser* browser,
    extensions::ExtensionId extension_id) {
  return extensions::ExtensionRegistry::Get(browser->profile())
      ->enabled_extensions()
      .GetByID(extension_id);
}

// Returns sorted extension ids based on their extensions name.
std::vector<std::string> SortExtensionsByName(
    ToolbarActionsModel& toolbar_model) {
  auto sort_by_name = [&toolbar_model](const ToolbarActionsModel::ActionId a,
                                       const ToolbarActionsModel::ActionId b) {
    return base::i18n::ToLower(toolbar_model.GetExtensionName(a)) <
           base::i18n::ToLower(toolbar_model.GetExtensionName(b));
  };
  std::vector<std::string> sorted_ids(toolbar_model.action_ids().begin(),
                                      toolbar_model.action_ids().end());
  std::sort(sorted_ids.begin(), sorted_ids.end(), sort_by_name);
  return sorted_ids;
}

// Returns the index of `action_id` in the toolbar model actions based on the
// extensions name alphabetical order.
size_t FindIndex(ToolbarActionsModel& toolbar_model,
                 const ToolbarActionsModel::ActionId& action_id) {
  std::u16string extension_name =
      base::i18n::ToLower(toolbar_model.GetExtensionName(action_id));
  auto sorted_action_ids = SortExtensionsByName(toolbar_model);
  return static_cast<size_t>(
      std::ranges::lower_bound(sorted_action_ids, extension_name, {},
                               [&toolbar_model](std::string id) {
                                 return base::i18n::ToLower(
                                     toolbar_model.GetExtensionName(id));
                               }) -
      sorted_action_ids.begin());
}

// Returns the main page, if it is the correct type.
ExtensionsMenuMainPageView* GetMainPage(views::View* page) {
  return views::AsViewClass<ExtensionsMenuMainPageView>(page);
}

// Returns the site permissions page, if it is the correct type.
ExtensionsMenuSitePermissionsPageView* GetSitePermissionsPage(
    views::View* page) {
  return views::AsViewClass<ExtensionsMenuSitePermissionsPageView>(page);
}

// Returns whether user can select the site access for `extension` on
// `web_contents`.
bool CanUserCustomizeExtensionSiteAccess(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  const GURL& url = web_contents.GetLastCommittedURL();
  if (toolbar_model.IsRestrictedUrl(url)) {
    // We don't allow customization of restricted sites (e.g.
    // chrome://settings).
    return false;
  }

  if (extension.permissions_data()->IsPolicyBlockedHost(url)) {
    // Users can't customize the site access of policy-blocked sites.
    return false;
  }

  if (extensions::ExtensionSystem::Get(&profile)
          ->management_policy()
          ->HasEnterpriseForcedAccess(extension)) {
    // Users can't customize the site access of enterprise-installed extensions.
    return false;
  }

  // The extension wants site access if it at least wants "on click" access.
  auto* permissions_manager = PermissionsManager::Get(&profile);
  bool extension_wants_access = permissions_manager->CanUserSelectSiteAccess(
      extension, url, PermissionsManager::UserSiteAccess::kOnClick);
  if (!extension_wants_access) {
    // Users can't customize site access of extensions that don't want access to
    // begin with.
    return false;
  }

  // Users can only customize site access when they have allowed all extensions
  // to be customizable on the site.
  return permissions_manager->GetUserSiteSetting(
             web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin()) ==
         PermissionsManager::UserSiteSetting::kCustomizeByExtension;
}

}  // namespace

ExtensionsMenuViewPlatformDelegateViews::
    ExtensionsMenuViewPlatformDelegateViews(
        Browser* browser,
        ExtensionsContainerViews* extensions_container,
        views::View* bubble_contents)
    : browser_(browser),
      extensions_container_(extensions_container),
      bubble_contents_(bubble_contents),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())) {
}

ExtensionsMenuViewPlatformDelegateViews::
    ~ExtensionsMenuViewPlatformDelegateViews() = default;

void ExtensionsMenuViewPlatformDelegateViews::AttachToModel(
    ExtensionsMenuViewModel* model) {
  CHECK(model);
  CHECK(!menu_model_);
  menu_model_ = model;
}

void ExtensionsMenuViewPlatformDelegateViews::DetachFromModel() {
  CHECK(menu_model_);
  menu_model_ = nullptr;
}

void ExtensionsMenuViewPlatformDelegateViews::OnActiveWebContentsChanged(
    content::WebContents* web_contents) {
  UpdatePage(web_contents);
}

void ExtensionsMenuViewPlatformDelegateViews::OnHostAccessRequestAddedOrUpdated(
    const extensions::ExtensionId& extension_id,
    content::WebContents* web_contents) {
  CHECK(current_page_);

  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  // TODO(crbug.com/330588494): Add to correct index based on alphabetic
  // order.
  int index = 0;
  AddOrUpdateExtensionRequestingAccess(main_page, extension_id, index,
                                       web_contents);
  main_page->MaybeShowRequestsSection();
}

void ExtensionsMenuViewPlatformDelegateViews::OnHostAccessRequestRemoved(
    const extensions::ExtensionId& extension_id) {
  CHECK(current_page_);

  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  main_page->RemoveExtensionRequestingAccess(extension_id);
  main_page->MaybeShowRequestsSection();
}

void ExtensionsMenuViewPlatformDelegateViews::OnHostAccessRequestsCleared() {
  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  main_page->ClearExtensionsRequestingAccess();
  main_page->MaybeShowRequestsSection();
}

void ExtensionsMenuViewPlatformDelegateViews::
    OnHostAccessRequestDismissedByUser(
        const extensions::ExtensionId& extension_id) {
  CHECK(current_page_);

  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  main_page->RemoveExtensionRequestingAccess(extension_id);
  main_page->MaybeShowRequestsSection();
}

void ExtensionsMenuViewPlatformDelegateViews::
    OnShowHostAccessRequestsInToolbarChanged(
        const extensions::ExtensionId& extension_id,
        bool can_show_requests) {
  CHECK(current_page_);

  // Changing whether an extension can show requests access in the toolbar only
  // affects the site permissions page ...
  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  if (!site_permissions_page) {
    return;
  }

  // ... of that extension.
  if (site_permissions_page->extension_id() == extension_id) {
    site_permissions_page->UpdateShowRequestsToggle(can_show_requests);
  }
}

void ExtensionsMenuViewPlatformDelegateViews::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(current_page_);

  // A new toolbar action only affects the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  int index = FindIndex(*toolbar_model_, action_id);
  InsertMenuItemMainPage(main_page, action_id, index);
}

void ExtensionsMenuViewPlatformDelegateViews::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(current_page_);

  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  if (site_permissions_page) {
    // Return to the main page if site permissions page belongs to the extension
    // removed.
    if (site_permissions_page->extension_id() == action_id) {
      OpenMainPage();
    }
    return;
  }

  // Remove the menu item for the extension when main page is opened.
  auto* main_page = GetMainPage(current_page_.view());
  CHECK(main_page);
  main_page->RemoveMenuItem(action_id);
}

void ExtensionsMenuViewPlatformDelegateViews::OnToolbarActionUpdated() {
  UpdatePage(GetActiveWebContents());
}

void ExtensionsMenuViewPlatformDelegateViews::OnToolbarModelInitialized() {
  CHECK(current_page_);

  // Toolbar model should have been initialized if site permissions page is
  // open, since this page can only be reached after main page was populated
  // after toolbar model was initialized.
  CHECK(!GetSitePermissionsPage(current_page_.view()));

  auto* main_page = GetMainPage(current_page_.view());
  CHECK(main_page);
  PopulateMainPage(main_page);
}

void ExtensionsMenuViewPlatformDelegateViews::OnToolbarPinnedActionsChanged() {
  CHECK(current_page_);

  // Do nothing when is not the main page, as site permissions page doesn't have
  // pinning functionality.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  std::vector<ExtensionMenuItemView*> menu_items = main_page->GetMenuItems();
  for (auto* menu_item : menu_items) {
    bool is_action_pinned =
        toolbar_model_->IsActionPinned(menu_item->view_model()->GetId());
    menu_item->UpdateContextMenuButton(is_action_pinned);
  }
}

void ExtensionsMenuViewPlatformDelegateViews::OnPermissionsSettingsChanged() {
  CHECK(current_page_);

  if (GetSitePermissionsPage(current_page_.view())) {
    // Site permissions page can only be opened when site setting is set to
    // "customize by extension". Thus, when site settings changed, we have to
    // return to main page.
    DCHECK_NE(PermissionsManager::Get(browser_->profile())
                  ->GetUserSiteSetting(GetActiveWebContents()
                                           ->GetPrimaryMainFrame()
                                           ->GetLastCommittedOrigin()),
              PermissionsManager::UserSiteSetting::kCustomizeByExtension);
    OpenMainPage();
    return;
  }

  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  CHECK(main_page);
  UpdateMainPage(main_page, GetActiveWebContents());

  // TODO(crbug.com/40879945): Update the "highlighted section" based on the
  // `site_setting` and whether a page refresh is needed.

  // TODO(crbug.com/40879945): Run blocked actions for extensions that only have
  // blocked actions that don't require a page refresh to run.
}

void ExtensionsMenuViewPlatformDelegateViews::OpenMainPage() {
  auto main_page = std::make_unique<ExtensionsMenuMainPageView>(browser_, this);
  UpdateMainPage(main_page.get(), GetActiveWebContents());
  PopulateMainPage(main_page.get());

  SwitchToPage(std::move(main_page));
}

void ExtensionsMenuViewPlatformDelegateViews::OpenSitePermissionsPage(
    const extensions::ExtensionId& extension_id) {
  CHECK(CanUserCustomizeExtensionSiteAccess(
      *GetExtension(browser_, extension_id), *browser_->profile(),
      *toolbar_model_, *GetActiveWebContents()));

  auto site_permissions_page =
      std::make_unique<ExtensionsMenuSitePermissionsPageView>(
          browser_, extension_id, this);
  UpdateSitePermissionsPage(site_permissions_page.get(),
                            GetActiveWebContents());

  SwitchToPage(std::move(site_permissions_page));

  base::RecordAction(
      base::UserMetricsAction("Extensions.Menu.SitePermissionsPageOpened"));
}

void ExtensionsMenuViewPlatformDelegateViews::CloseBubble() {
  bubble_contents_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void ExtensionsMenuViewPlatformDelegateViews::OnSiteAccessSelected(
    const extensions::ExtensionId& extension_id,
    PermissionsManager::UserSiteAccess site_access) {
  menu_model_->UpdateSiteAccess(extension_id, site_access);
}

void ExtensionsMenuViewPlatformDelegateViews::OnSiteSettingsToggleButtonPressed(
    bool is_on) {
  PermissionsManager::UserSiteSetting site_setting =
      is_on ? PermissionsManager::UserSiteSetting::kCustomizeByExtension
            : PermissionsManager::UserSiteSetting::kBlockAllExtensions;
  menu_model_->UpdateSiteSetting(site_setting);
}

void ExtensionsMenuViewPlatformDelegateViews::OnExtensionToggleSelected(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  if (is_on) {
    menu_model_->GrantSiteAccess(extension_id);
  } else {
    menu_model_->RevokeSiteAccess(extension_id);
  }
}

void ExtensionsMenuViewPlatformDelegateViews::OnReloadPageButtonClicked() {
  menu_model_->ReloadWebContents();
}

void ExtensionsMenuViewPlatformDelegateViews::OnAllowExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  menu_model_->AllowHostAccessRequest(extension_id);
}

void ExtensionsMenuViewPlatformDelegateViews::OnDismissExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  menu_model_->DismissHostAccessRequest(extension_id);
}

void ExtensionsMenuViewPlatformDelegateViews::OnShowRequestsTogglePressed(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  menu_model_->ShowHostAccessRequestsInToolbar(extension_id, is_on);
}

void ExtensionsMenuViewPlatformDelegateViews::UpdatePage(
    content::WebContents* web_contents) {
  DCHECK(current_page_);

  if (!web_contents) {
    return;
  }

  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  if (site_permissions_page) {
    // Update site permissions page if the extension can have one.
    if (CanUserCustomizeExtensionSiteAccess(
            *GetExtension(browser_, site_permissions_page->extension_id()),
            *browser_->profile(), *toolbar_model_, *web_contents)) {
      UpdateSitePermissionsPage(site_permissions_page, web_contents);
      return;
    }

    // Otherwise navigate back to the main page.
    OpenMainPage();
    return;
  }

  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  DCHECK(main_page);
  UpdateMainPage(main_page, web_contents);
}

void ExtensionsMenuViewPlatformDelegateViews::UpdateMainPage(
    ExtensionsMenuMainPageView* main_page,
    content::WebContents* web_contents) {
  CHECK(web_contents);
  auto has_enterprise_extensions = [&]() {
    return std::any_of(
        toolbar_model_->action_ids().begin(),
        toolbar_model_->action_ids().end(),
        [this](const ToolbarActionsModel::ActionId extension_id) {
          auto* extension = GetExtension(browser_, extension_id);
          return extensions::ExtensionSystem::Get(browser_->profile())
              ->management_policy()
              ->HasEnterpriseForcedAccess(*extension);
        });
  };
  auto reload_required = [web_contents]() {
    return extensions::TabHelper::FromWebContents(web_contents)
        ->IsReloadRequired();
  };

  std::u16string current_site =
      extensions::ui_util::GetFormattedHostForDisplay(*web_contents);
  int site_settings_label_id;
  bool is_site_settings_toggle_visible = false;
  bool is_site_settings_toggle_on = false;
  bool is_site_settings_tooltip_visible = false;
  bool is_reload_required = false;
  bool can_have_requests = false;

  MainPageState state =
      GetMainPageState(*browser_->profile(), *toolbar_model_, *web_contents);
  switch (state) {
    case MainPageState::kRestrictedSite:
      site_settings_label_id =
          IDS_EXTENSIONS_MENU_SITE_SETTINGS_NOT_ALLOWED_LABEL;
      is_site_settings_toggle_visible = false;
      is_site_settings_toggle_on = false;
      break;
    case MainPageState::kPolicyBlockedSite:
      site_settings_label_id =
          IDS_EXTENSIONS_MENU_SITE_SETTINGS_NOT_ALLOWED_LABEL;
      is_site_settings_toggle_visible = false;
      is_site_settings_toggle_on = false;
      is_site_settings_tooltip_visible = has_enterprise_extensions();
      break;
    case MainPageState::kUserBlockedSite:
      site_settings_label_id = IDS_EXTENSIONS_MENU_SITE_SETTINGS_LABEL;
      is_site_settings_toggle_visible = true;
      is_site_settings_toggle_on = false;
      is_site_settings_tooltip_visible = has_enterprise_extensions();
      is_reload_required = reload_required();
      break;
    case MainPageState::kUserCustomizedSite:
      site_settings_label_id = IDS_EXTENSIONS_MENU_SITE_SETTINGS_LABEL;
      is_site_settings_toggle_visible = true;
      is_site_settings_toggle_on = true;
      is_reload_required = reload_required();
      can_have_requests = true;
      break;
  }

  main_page->UpdateSiteSettings(
      current_site, site_settings_label_id, is_site_settings_tooltip_visible,
      is_site_settings_toggle_visible, is_site_settings_toggle_on);

  if (is_reload_required) {
    main_page->ShowReloadSection();
  } else if (can_have_requests) {
    int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
    auto* permissions_manager = PermissionsManager::Get(browser_->profile());
    int index = 0;
    std::vector<std::string> extension_ids =
        SortExtensionsByName(*toolbar_model_);

    for (const auto& extension_id : extension_ids) {
      if (permissions_manager->HasActiveHostAccessRequest(tab_id,
                                                          extension_id)) {
        AddOrUpdateExtensionRequestingAccess(main_page, extension_id, index,
                                             web_contents);
        ++index;
      } else {
        // Otherwise remove its entry, if existent.
        main_page->RemoveExtensionRequestingAccess(extension_id);
      }
    }
    main_page->MaybeShowRequestsSection();
  }

  // Update menu items.
  // TODO(crbug.com/40879945): Reorder the extensions after updating them, since
  // their names can change.
  std::vector<ExtensionMenuItemView*> menu_items = main_page->GetMenuItems();
  for (auto* menu_item : menu_items) {
    const extensions::Extension* extension =
        GetExtension(browser_, menu_item->view_model()->GetId());
    CHECK(extension);

    ExtensionsMenuViewModel::MenuItemInfo menu_item_info =
        menu_model_->GetMenuItemInfo(menu_item->view_model());
    menu_item->Update(menu_item_info);
  }
}

void ExtensionsMenuViewPlatformDelegateViews::UpdateSitePermissionsPage(
    ExtensionsMenuSitePermissionsPageView* site_permissions_page,
    content::WebContents* web_contents) {
  CHECK(web_contents);

  auto* permissions_manager = PermissionsManager::Get(browser_->profile());
  SitePermissionsHelper permissions_helper(browser_->profile());

  extensions::ExtensionId extension_id = site_permissions_page->extension_id();
  const extensions::Extension* extension = GetExtension(browser_, extension_id);
  const GURL& url = web_contents->GetLastCommittedURL();
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  ToolbarActionViewModel* view_model =
      extensions_container_->GetActionForId(extension_id);

  std::u16string extension_name = view_model->GetActionName();
  ui::ImageModel extension_icon =
      view_model->GetIcon(web_contents, gfx::Size(icon_size, icon_size));
  std::u16string current_site =
      extensions::ui_util::GetFormattedHostForDisplay(*web_contents);
  PermissionsManager::UserSiteAccess user_site_access =
      permissions_manager->GetUserSiteAccess(*extension, url);
  bool is_show_requests_toggle_on =
      permissions_helper.ShowAccessRequestsInToolbar(extension_id);
  bool is_on_site_enabled = permissions_manager->CanUserSelectSiteAccess(
      *extension, url, PermissionsManager::UserSiteAccess::kOnSite);
  bool is_on_all_sites_enabled = permissions_manager->CanUserSelectSiteAccess(
      *extension, url, PermissionsManager::UserSiteAccess::kOnAllSites);

  site_permissions_page->Update(extension_name, extension_icon, current_site,
                                user_site_access, is_show_requests_toggle_on,
                                is_on_site_enabled, is_on_all_sites_enabled);
}

ExtensionsMenuMainPageView*
ExtensionsMenuViewPlatformDelegateViews::GetMainPageViewForTesting() {
  DCHECK(current_page_);
  return GetMainPage(current_page_.view());
}

ExtensionsMenuSitePermissionsPageView*
ExtensionsMenuViewPlatformDelegateViews::GetSitePermissionsPageForTesting() {
  DCHECK(current_page_);
  return GetSitePermissionsPage(current_page_.view());
}

void ExtensionsMenuViewPlatformDelegateViews::SwitchToPage(
    std::unique_ptr<views::View> page) {
  if (current_page_) {
    bubble_contents_->RemoveChildViewT(current_page_.view());
  }
  DCHECK(!current_page_);
  current_page_.SetView(bubble_contents_->AddChildView(std::move(page)));
}

void ExtensionsMenuViewPlatformDelegateViews::PopulateMainPage(
    ExtensionsMenuMainPageView* main_page) {
  // TODO(crbug.com/40879945): We should update the subheader here since it
  // depends on `toolbar_model_`.
  std::vector<std::string> sorted_ids = SortExtensionsByName(*toolbar_model_);
  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    InsertMenuItemMainPage(main_page, sorted_ids[i], i);
  }
}

void ExtensionsMenuViewPlatformDelegateViews::InsertMenuItemMainPage(
    ExtensionsMenuMainPageView* main_page,
    const extensions::ExtensionId& extension_id,
    int index) {
  std::unique_ptr<ExtensionActionViewModel> model =
      ExtensionActionViewModel::Create(
          extension_id, browser_,
          std::make_unique<ExtensionActionPlatformDelegateViews>(
              browser_, extensions_container_));

  ExtensionsMenuViewModel::MenuItemInfo menu_item =
      menu_model_->GetMenuItemInfo(model.get());

  main_page->CreateAndInsertMenuItem(std::move(model), extension_id, menu_item,
                                     index);
}

void ExtensionsMenuViewPlatformDelegateViews::
    AddOrUpdateExtensionRequestingAccess(
        ExtensionsMenuMainPageView* main_page,
        const extensions::ExtensionId& extension_id,
        int index,
        content::WebContents* web_contents) {
  ToolbarActionViewModel* view_model =
      extensions_container_->GetActionForId(extension_id);
  std::u16string name = view_model->GetActionName();
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  ui::ImageModel icon =
      view_model->GetIcon(web_contents, gfx::Size(icon_size, icon_size));

  main_page->AddOrUpdateExtensionRequestingAccess(extension_id, name, icon,
                                                  index);
}

content::WebContents*
ExtensionsMenuViewPlatformDelegateViews::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
