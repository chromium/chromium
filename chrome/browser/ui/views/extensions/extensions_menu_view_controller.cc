// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_id.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;
using SitePermissionsHelper = extensions::SitePermissionsHelper;

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
      base::ranges::lower_bound(sorted_action_ids, extension_name, {},
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

// Returns whether `extension` cannot have its site access modified by the user
// because of policy.
bool HasEnterpriseForcedAccess(const extensions::Extension& extension,
                               Profile& profile) {
  extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(&profile)->management_policy();
  return !policy->UserMayModifySettings(&extension, nullptr) ||
         policy->MustRemainInstalled(&extension, nullptr);
}

// Returns whether the site setting toggle for `web_contents` should be visible.
bool IsSiteSettingsToggleVisible(const ToolbarActionsModel& toolbar_model,
                                 content::WebContents* web_contents) {
  return !toolbar_model.IsRestrictedUrl(web_contents->GetLastCommittedURL());
}

// Returns whether the site settings toggle for `web_contents` should be on.
bool IsSiteSettingsToggleOn(Browser* browser,
                            content::WebContents* web_contents) {
  auto origin = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  return PermissionsManager::Get(browser->profile())
             ->GetUserSiteSetting(origin) ==
         PermissionsManager::UserSiteSetting::kCustomizeByExtension;
}

// Returns whether the site permissions button should be visible.
bool IsSitePermissionsButtonVisible(const extensions::Extension& extension,
                                    Profile& profile,
                                    const ToolbarActionsModel& toolbar_model,
                                    content::WebContents& web_contents) {
  // Button is never visible when site is restricted.
  if (toolbar_model.IsRestrictedUrl(web_contents.GetLastCommittedURL())) {
    return false;
  }

  PermissionsManager::UserSiteSetting user_site_setting =
      PermissionsManager::Get(&profile)->GetUserSiteSetting(
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin());
  switch (user_site_setting) {
    case PermissionsManager::UserSiteSetting::kCustomizeByExtension: {
      // Extensions should always display the button.
      return true;
    }
    case PermissionsManager::UserSiteSetting::kBlockAllExtensions: {
      // Extension should only display the button when it's an enterprise
      // extension and has granted access.
      bool enterprise_forced_access =
          HasEnterpriseForcedAccess(extension, profile);
      SitePermissionsHelper::SiteInteraction site_interaction =
          SitePermissionsHelper(&profile).GetSiteInteraction(extension,
                                                             &web_contents);
      return enterprise_forced_access &&
             site_interaction ==
                 SitePermissionsHelper::SiteInteraction::kGranted;
    }
    case PermissionsManager::UserSiteSetting::kGrantAllExtensions: {
      NOTREACHED_NORETURN();
    }
  }
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

  bool enterprise_forced_access = HasEnterpriseForcedAccess(extension, profile);
  if (enterprise_forced_access) {
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

// Returns the state for the `extension`'s site permissions button.
ExtensionMenuItemView::SitePermissionsButtonState GetSitePermissionsButtonState(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  bool is_site_permissions_button_visible = IsSitePermissionsButtonVisible(
      extension, profile, toolbar_model, web_contents);
  if (!is_site_permissions_button_visible) {
    return ExtensionMenuItemView::SitePermissionsButtonState::kHidden;
  }

  bool is_site_permissions_button_enabled = CanUserCustomizeExtensionSiteAccess(
      extension, profile, toolbar_model, web_contents);
  return is_site_permissions_button_enabled
             ? ExtensionMenuItemView::SitePermissionsButtonState::kEnabled
             : ExtensionMenuItemView::SitePermissionsButtonState::kDisabled;
}

// Returns the state for the `extension`'s site access toggle button.
ExtensionMenuItemView::SiteAccessToggleState GetSiteAccessToggleState(
    const extensions::Extension& extension,
    Profile& profile,
    const ToolbarActionsModel& toolbar_model,
    content::WebContents& web_contents) {
  if (!CanUserCustomizeExtensionSiteAccess(extension, profile, toolbar_model,
                                           web_contents)) {
    return ExtensionMenuItemView::SiteAccessToggleState::kHidden;
  }

  PermissionsManager::UserSiteAccess site_access =
      PermissionsManager::Get(&profile)->GetUserSiteAccess(
          extension, web_contents.GetLastCommittedURL());
  return site_access == PermissionsManager::UserSiteAccess::kOnClick
             ? ExtensionMenuItemView::SiteAccessToggleState::kOff
             : ExtensionMenuItemView::SiteAccessToggleState::kOn;
}

}  // namespace

ExtensionsMenuViewController::ExtensionsMenuViewController(
    Browser* browser,
    ExtensionsContainer* extensions_container,
    views::View* bubble_contents,
    views::BubbleDialogDelegate* bubble_delegate)
    : browser_(browser),
      extensions_container_(extensions_container),
      bubble_contents_(bubble_contents),
      bubble_delegate_(bubble_delegate),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())) {
  browser_->tab_strip_model()->AddObserver(this);
  toolbar_model_observation_.Observe(toolbar_model_.get());
  permissions_manager_observation_.Observe(
      PermissionsManager::Get(browser_->profile()));
}

ExtensionsMenuViewController::~ExtensionsMenuViewController() {
  // Note: No need to call TabStripModel::RemoveObserver(), because it's handled
  // directly within TabStripModelObserver::~TabStripModelObserver().
}

void ExtensionsMenuViewController::OpenMainPage() {
  auto main_page = std::make_unique<ExtensionsMenuMainPageView>(browser_, this);
  UpdateMainPage(main_page.get(), GetActiveWebContents());
  PopulateMainPage(main_page.get());

  SwitchToPage(std::move(main_page));
}

void ExtensionsMenuViewController::OpenSitePermissionsPage(
    extensions::ExtensionId extension_id) {
  CHECK(CanUserCustomizeExtensionSiteAccess(
      *GetExtension(browser_, extension_id), *browser_->profile(),
      *toolbar_model_, *GetActiveWebContents()));

  auto site_permissions_page =
      std::make_unique<ExtensionsMenuSitePermissionsPageView>(
          browser_, extension_id, this);
  UpdateSitePermissionsPage(site_permissions_page.get(),
                            GetActiveWebContents());

  SwitchToPage(std::move(site_permissions_page));
}

void ExtensionsMenuViewController::CloseBubble() {
  bubble_contents_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void ExtensionsMenuViewController::OnSiteAccessSelected(
    extensions::ExtensionId extension_id,
    PermissionsManager::UserSiteAccess site_access) {
  SitePermissionsHelper permissions(browser_->profile());
  permissions.UpdateSiteAccess(*GetExtension(browser_, extension_id),
                               GetActiveWebContents(), site_access);
}

void ExtensionsMenuViewController::TabChangedAt(content::WebContents* contents,
                                                int index,
                                                TabChangeType change_type) {
  UpdatePage(contents);
}

void ExtensionsMenuViewController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  if (!selection.active_tab_changed() || !web_contents) {
    return;
  }

  UpdatePage(GetActiveWebContents());
}

void ExtensionsMenuViewController::UpdatePage(
    content::WebContents* web_contents) {
  DCHECK(current_page_);

  if (!web_contents) {
    return;
  }

  auto* site_permissions_page = GetSitePermissionsPage(current_page_);
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

  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_);
  DCHECK(main_page);
  UpdateMainPage(main_page, web_contents);
}

void ExtensionsMenuViewController::UpdateMainPage(
    ExtensionsMenuMainPageView* main_page,
    content::WebContents* web_contents) {
  CHECK(web_contents);

  std::u16string current_site = GetCurrentHost(web_contents);
  bool is_site_settings_toggle_visible =
      IsSiteSettingsToggleVisible(*toolbar_model_, web_contents);
  bool is_site_settings_toggle_on =
      IsSiteSettingsToggleOn(browser_, web_contents);
  main_page->Update(current_site, is_site_settings_toggle_visible,
                    is_site_settings_toggle_on);

  std::vector<ExtensionMenuItemView*> menu_items = main_page->GetMenuItems();
  for (auto* menu_item : menu_items) {
    const extensions::Extension* extension =
        GetExtension(browser_, menu_item->view_controller()->GetId());
    CHECK(extension);

    ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state =
        GetSiteAccessToggleState(*extension, *browser_->profile(),
                                 *toolbar_model_, *web_contents);
    ExtensionMenuItemView::SitePermissionsButtonState
        site_permissions_button_state = GetSitePermissionsButtonState(
            *extension, *browser_->profile(), *toolbar_model_, *web_contents);
    menu_item->Update(site_access_toggle_state, site_permissions_button_state);
  }
}

void ExtensionsMenuViewController::UpdateSitePermissionsPage(
    ExtensionsMenuSitePermissionsPageView* site_permissions_page,
    content::WebContents* web_contents) {
  CHECK(web_contents);

  extensions::ExtensionId extension_id = site_permissions_page->extension_id();
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  std::unique_ptr<ExtensionActionViewController> action_controller =
      ExtensionActionViewController::Create(extension_id, browser_,
                                            extensions_container_);

  std::u16string extension_name = action_controller->GetActionName();
  ui::ImageModel extension_icon = action_controller->GetIcon(
      GetActiveWebContents(), gfx::Size(icon_size, icon_size));
  std::u16string current_site = GetCurrentHost(web_contents);
  extensions::PermissionsManager::UserSiteAccess user_site_access =
      PermissionsManager::Get(browser_->profile())
          ->GetUserSiteAccess(*GetExtension(browser_, extension_id),
                              GetActiveWebContents()->GetLastCommittedURL());
  bool is_show_requests_toggle_on =
      extensions::SitePermissionsHelper(browser_->profile())
          .ShowAccessRequestsInToolbar(extension_id);

  site_permissions_page->Update(extension_name, extension_icon, current_site,
                                user_site_access, is_show_requests_toggle_on);
}

void ExtensionsMenuViewController::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  DCHECK(current_page_);

  // Do nothing when site permission page is opened as a new extension doesn't
  // affect the site permissions page of another extension.
  if (GetSitePermissionsPage(current_page_)) {
    return;
  }

  // Insert a menu item for the extension when main page is opened.
  auto* main_page = GetMainPage(current_page_);
  DCHECK(main_page);

  int index = FindIndex(*toolbar_model_, action_id);
  std::unique_ptr<ExtensionActionViewController> action_controller =
      ExtensionActionViewController::Create(action_id, browser_,
                                            extensions_container_);
  ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state =
      GetSiteAccessToggleState(*action_controller->extension(),
                               *browser_->profile(), *toolbar_model_,
                               *GetActiveWebContents());
  ExtensionMenuItemView::SitePermissionsButtonState
      site_permissions_button_state = GetSitePermissionsButtonState(
          *action_controller->extension(), *browser_->profile(),
          *toolbar_model_, *GetActiveWebContents());

  main_page->CreateAndInsertMenuItem(std::move(action_controller), action_id,
                                     site_access_toggle_state,
                                     site_permissions_button_state, index);

  // TODO(crbug.com/1390952): Update requests access section once such section
  // is implemented (if the extension added requests site access, it needs to be
  // added to such section).
  bubble_delegate_->SizeToContents();
}

void ExtensionsMenuViewController::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  DCHECK(current_page_);

  auto* site_permissions_page = GetSitePermissionsPage(current_page_);
  if (site_permissions_page) {
    // Return to the main page if site permissions page belongs to the extension
    // removed.
    if (site_permissions_page->extension_id() == action_id) {
      OpenMainPage();
    }
    return;
  }

  // Remove the menu item for the extension when main page is opened.
  auto* main_page = GetMainPage(current_page_);
  DCHECK(main_page);
  main_page->RemoveMenuItem(action_id);

  // TODO(crbug.com/1390952): Update requests access section (if the extension
  // removed was in the section, it needs to be removed).
  bubble_delegate_->SizeToContents();
}

void ExtensionsMenuViewController::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  UpdatePage(GetActiveWebContents());
}

void ExtensionsMenuViewController::OnToolbarModelInitialized() {
  DCHECK(current_page_);

  // Toolbar model should have been initialized if site permissions page is
  // open, since this page can only be reached after main page was populated
  // after toolbar model was initialized.
  CHECK(!GetSitePermissionsPage(current_page_));

  auto* main_page = GetMainPage(current_page_);
  DCHECK(main_page);
  PopulateMainPage(main_page);
}

void ExtensionsMenuViewController::OnToolbarPinnedActionsChanged() {
  DCHECK(current_page_);

  // Do nothing when site permissions page is opened as it doesn't have pin
  // buttons.
  if (GetSitePermissionsPage(current_page_)) {
    return;
  }

  auto* main_page = GetMainPage(current_page_);
  DCHECK(main_page);

  std::vector<ExtensionMenuItemView*> menu_items = main_page->GetMenuItems();
  for (auto* menu_item : menu_items) {
    bool is_action_pinned =
        toolbar_model_->IsActionPinned(menu_item->view_controller()->GetId());
    menu_item->UpdateContextMenuButton(is_action_pinned);
  }
}

void ExtensionsMenuViewController::OnUserPermissionsSettingsChanged(
    const PermissionsManager::UserPermissionsSettings& settings) {
  DCHECK(current_page_);

  if (GetSitePermissionsPage(current_page_)) {
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

  DCHECK(GetMainPage(current_page_));
  UpdatePage(GetActiveWebContents());

  // TODO(crbug.com/1390952): Update the "highlighted section" based on the
  // `site_setting` and whether a page refresh is needed.

  // TODO(crbug.com/1390952): Run blocked actions for extensions that only have
  // blocked actions that don't require a page refresh to run.
}

void ExtensionsMenuViewController::OnShowAccessRequestsInToolbarChanged(
    const extensions::ExtensionId& extension_id,
    bool can_show_requests) {
  DCHECK(current_page_);

  // Changing whether an extension can show requests access in the toolbar only
  // affects the site permissions page for such extension.
  auto* site_permissions_page = GetSitePermissionsPage(current_page_);
  if (site_permissions_page &&
      site_permissions_page->extension_id() == extension_id) {
    site_permissions_page->UpdateShowRequestsToggle(can_show_requests);
  }
}

void ExtensionsMenuViewController::OnViewIsDeleting(
    views::View* observed_view) {
  DCHECK_EQ(observed_view, current_page_);
  current_page_ = nullptr;
}

ExtensionsMenuMainPageView*
ExtensionsMenuViewController::GetMainPageViewForTesting() {
  DCHECK(current_page_);
  return GetMainPage(current_page_);
}

ExtensionsMenuSitePermissionsPageView*
ExtensionsMenuViewController::GetSitePermissionsPageForTesting() {
  DCHECK(current_page_);
  return GetSitePermissionsPage(current_page_);
}

void ExtensionsMenuViewController::SwitchToPage(
    std::unique_ptr<views::View> page) {
  if (current_page_) {
    bubble_contents_->RemoveChildViewT(current_page_.get());
  }
  DCHECK(!current_page_);
  current_page_ = bubble_contents_->AddChildView(std::move(page));
  current_page_->AddObserver(this);

  // Only resize the menu if the bubble is created, since page could be added to
  // the menu beforehand and delegate wouldn't know the bubble bounds.
  if (bubble_delegate_->GetBubbleFrameView()) {
    bubble_delegate_->SizeToContents();
  }
}

void ExtensionsMenuViewController::PopulateMainPage(
    ExtensionsMenuMainPageView* main_page) {
  std::vector<std::string> sorted_ids = SortExtensionsByName(*toolbar_model_);
  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    // TODO(emiliapaz): Under MVC architecture, view should not own the view
    // controller. However, the current extensions structure depends on this
    // thus a major restructure is needed.
    std::unique_ptr<ExtensionActionViewController> action_controller =
        ExtensionActionViewController::Create(sorted_ids[i], browser_,
                                              extensions_container_);
    ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state =
        GetSiteAccessToggleState(*action_controller->extension(),
                                 *browser_->profile(), *toolbar_model_,
                                 *GetActiveWebContents());
    ExtensionMenuItemView::SitePermissionsButtonState
        site_permissions_button_state = GetSitePermissionsButtonState(
            *action_controller->extension(), *browser_->profile(),
            *toolbar_model_, *GetActiveWebContents());

    main_page->CreateAndInsertMenuItem(std::move(action_controller),
                                       sorted_ids[i], site_access_toggle_state,
                                       site_permissions_button_state, i);
  }
}

content::WebContents* ExtensionsMenuViewController::GetActiveWebContents()
    const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
