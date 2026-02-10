// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_delegate_desktop.h"

#include <algorithm>

#include "base/check_deref.h"
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
#include "chrome/browser/ui/views/extensions/extension_action_delegate_desktop.h"
#include "chrome/browser/ui/views/extensions/extension_view_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
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

// Returns whether host access requests can be visible in the menu.
bool CanShowHostAccessRequests(
    ExtensionsMenuViewModel::OptionalSection optional_section) {
  // Requests are only visible in the 'host access requests' section.
  return optional_section ==
         ExtensionsMenuViewModel::OptionalSection::kHostAccessRequests;
}

// Returns the size of the extension icon displayed in the menu.
gfx::Size GetMenuExtensionIconSize() {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  return gfx::Size(icon_size, icon_size);
}

// Returns the host access request info for `extension_id`.
ExtensionsMenuViewModel::HostAccessRequest GetHostAccessRequest(
    ExtensionsMenuViewModel& menu_model,
    const extensions::ExtensionId& extension_id) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  ExtensionsMenuViewModel::HostAccessRequest request =
      menu_model.GetHostAccessRequest(extension_id,
                                      gfx::Size(icon_size, icon_size));
  return request;
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

}  // namespace

ExtensionsMenuDelegateDesktop::ExtensionsMenuDelegateDesktop(
    Browser* browser,
    ExtensionsContainer* extensions_container,
    ExtensionsContainerViews* extensions_container_views,
    views::View* bubble_contents)
    : browser_(browser),
      extensions_container_(CHECK_DEREF(extensions_container)),
      extensions_container_views_(extensions_container_views),
      bubble_contents_(bubble_contents),
      menu_model_(std::make_unique<ExtensionsMenuViewModel>(browser_,
                                                            /*delegate=*/this)),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())) {
  menu_model_observation_.Observe(menu_model_.get());
}

ExtensionsMenuDelegateDesktop::~ExtensionsMenuDelegateDesktop() = default;

std::unique_ptr<ExtensionActionViewModel>
ExtensionsMenuDelegateDesktop::CreateActionViewModel(
    const extensions::ExtensionId& extension_id) {
  return ExtensionActionViewModel::Create(
      extension_id, browser_,
      std::make_unique<ExtensionActionDelegateDesktop>(
          browser_, &extensions_container_.get(), extensions_container_views_));
}

void ExtensionsMenuDelegateDesktop::OnPageNavigation() {
  DCHECK(current_page_);

  // Update main page if it is open.
  auto* main_page = GetMainPage(current_page_.view());
  if (main_page) {
    UpdateMainPage(main_page);
    return;
  }

  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  CHECK(site_permissions_page);
  if (menu_model_->CanShowSitePermissionsPage(
          site_permissions_page->extension_id())) {
    // Update site permissions page if it is open and the extension can have
    // one.
    UpdateSitePermissionsPage(site_permissions_page);
  } else {
    // Otherwise navigate back to the main page.
    OpenMainPage();
  }
}

void ExtensionsMenuDelegateDesktop::OnHostAccessRequestAdded(
    const extensions::ExtensionId& extension_id,
    int index) {
  CHECK(current_page_);

  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  // Requests are only visible in the 'host access requests' section.
  ExtensionsMenuViewModel::OptionalSection optional_section =
      menu_model_->GetOptionalSection();
  if (!CanShowHostAccessRequests(optional_section)) {
    return;
  }

  ExtensionsMenuViewModel::HostAccessRequest request =
      GetHostAccessRequest(*menu_model_, extension_id);
  main_page->AddExtensionRequestingAccess(request, index);
  main_page->SetOptionalSectionVisibility(optional_section);
}

void ExtensionsMenuDelegateDesktop::OnHostAccessRequestUpdated(
    const extensions::ExtensionId& extension_id,
    int index) {
  CHECK(current_page_);

  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  // Requests are only visible in the 'host access requests' section.
  ExtensionsMenuViewModel::OptionalSection optional_section =
      menu_model_->GetOptionalSection();
  if (!CanShowHostAccessRequests(optional_section)) {
    return;
  }

  ExtensionsMenuViewModel::HostAccessRequest request =
      GetHostAccessRequest(*menu_model_, extension_id);
  main_page->UpdateExtensionRequestingAccess(request, index);
  main_page->SetOptionalSectionVisibility(optional_section);
}

void ExtensionsMenuDelegateDesktop::OnHostAccessRequestRemoved(
    const extensions::ExtensionId& extension_id,
    int index) {
  CHECK(current_page_);

  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  // Requests are only visible in the 'host access requests' section.
  ExtensionsMenuViewModel::OptionalSection optional_section =
      menu_model_->GetOptionalSection();
  if (!CanShowHostAccessRequests(optional_section)) {
    return;
  }

  main_page->RemoveExtensionRequestingAccess(extension_id, index);
  main_page->SetOptionalSectionVisibility(optional_section);
}

void ExtensionsMenuDelegateDesktop::OnHostAccessRequestsCleared() {
  // Site access requests only affect the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  ExtensionsMenuViewModel::OptionalSection optional_section =
      menu_model_->GetOptionalSection();
  if (!CanShowHostAccessRequests(optional_section)) {
    return;
  }

  main_page->ClearExtensionsRequestingAccess();
  main_page->SetOptionalSectionVisibility(optional_section);
}

void ExtensionsMenuDelegateDesktop::OnShowHostAccessRequestsInToolbarChanged(
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
    site_permissions_page->UpdateShowRequestsToggle(
        menu_model_->GetExtensionShowRequestsToggleState(extension_id));
  }
}

void ExtensionsMenuDelegateDesktop::OnActionAdded(
    ExtensionActionViewModel* action_model,
    int index) {
  CHECK(current_page_);

  // A new toolbar action only affects the main page.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  InsertMenuEntry(main_page, action_model, index);
}

void ExtensionsMenuDelegateDesktop::OnActionRemoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
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

  // Remove the menu entry for the extension when main page is opened.
  auto* main_page = GetMainPage(current_page_.view());
  CHECK(main_page);
  main_page->RemoveMenuEntry(index);
}

void ExtensionsMenuDelegateDesktop::OnActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(current_page_);

  // Update the main page if it is open since an action update can affect the
  // whole main page, and not just its menu entry.
  auto* main_page = GetMainPage(current_page_.view());
  if (main_page) {
    UpdateMainPage(main_page);
    return;
  }

  // Do nothing when the site permissions page is opened for a different
  // extension.
  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  CHECK(site_permissions_page);
  if (site_permissions_page->extension_id() != action_id) {
    return;
  }

  // Update the site permissions page if it can be shown for the updated action,
  // otherwise go back to the main page.
  if (menu_model_->CanShowSitePermissionsPage(action_id)) {
    UpdateSitePermissionsPage(site_permissions_page);
  } else {
    OpenMainPage();
  }
}

void ExtensionsMenuDelegateDesktop::OnActionIconUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  CHECK(current_page_);

  // Update the icon for the extension entry in the main page.
  auto* main_page = GetMainPage(current_page_.view());
  if (main_page) {
    const auto& menu_entries = main_page->GetMenuEntries();
    auto it = std::find_if(menu_entries.begin(), menu_entries.end(),
                           [&action_id](auto* entry) {
                             return entry->extension_id() == action_id;
                           });

    if (it != menu_entries.end()) {
      (*it)->UpdateActionButton(menu_model_->GetActionButtonState(
          action_id, GetMenuExtensionIconSize()));
    }
    return;
  }

  // Do nothing when the site permissions page is opened for a different
  // extension.
  auto* site_permissions_page = GetSitePermissionsPage(current_page_.view());
  CHECK(site_permissions_page);
  if (site_permissions_page->extension_id() != action_id) {
    return;
  }

  // Update the icon for the extension's site permission page
  // TODO(crbug.com/431902556): consider updating only the icon and not the
  // whole site permissions page.
  UpdateSitePermissionsPage(site_permissions_page);
}

void ExtensionsMenuDelegateDesktop::OnActionsInitialized() {
  CHECK(current_page_);

  // Toolbar model should have been initialized if site permissions page is
  // open, since this page can only be reached after main page was populated
  // after toolbar model was initialized.
  CHECK(!GetSitePermissionsPage(current_page_.view()));

  auto* main_page = GetMainPage(current_page_.view());
  CHECK(main_page);
  PopulateMainPage(main_page);
}

void ExtensionsMenuDelegateDesktop::OnToolbarPinnedActionsChanged() {
  CHECK(current_page_);

  // Do nothing when is not the main page, as site permissions page doesn't have
  // pinning functionality.
  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  if (!main_page) {
    return;
  }

  std::vector<ExtensionsMenuEntryView*> menu_entries =
      main_page->GetMenuEntries();
  for (auto* menu_entry : menu_entries) {
    auto button_state =
        menu_model_->GetContextMenuButtonState(menu_entry->extension_id());
    menu_entry->UpdateContextMenuButton(button_state);
  }
}

void ExtensionsMenuDelegateDesktop::OnUserPermissionsSettingsChanged() {
  CHECK(current_page_);

  if (GetSitePermissionsPage(current_page_.view())) {
    // Site permissions page can only be opened when site setting is set to
    // "customize by extension". Thus, when site settings changed, we have to
    // return to main page.
    DCHECK_NE(PermissionsManager::Get(browser_->profile())
                  ->GetUserSiteSetting(browser_->tab_strip_model()
                                           ->GetActiveWebContents()
                                           ->GetPrimaryMainFrame()
                                           ->GetLastCommittedOrigin()),
              PermissionsManager::UserSiteSetting::kCustomizeByExtension);
    OpenMainPage();
    return;
  }

  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_.view());
  CHECK(main_page);
  UpdateMainPage(main_page);

  // TODO(crbug.com/40879945): Update the "highlighted section" based on the
  // `site_setting` and whether a page refresh is needed.

  // TODO(crbug.com/40879945): Run blocked actions for extensions that only have
  // blocked actions that don't require a page refresh to run.
}

void ExtensionsMenuDelegateDesktop::OpenMainPage() {
  auto main_page = std::make_unique<ExtensionsMenuMainPageView>(browser_, this);
  UpdateMainPage(main_page.get());
  PopulateMainPage(main_page.get());

  SwitchToPage(std::move(main_page));
}

void ExtensionsMenuDelegateDesktop::OpenSitePermissionsPage(
    const extensions::ExtensionId& extension_id) {
  CHECK(menu_model_->CanShowSitePermissionsPage(extension_id));

  auto site_permissions_page =
      std::make_unique<ExtensionsMenuSitePermissionsPageView>(
          browser_, extension_id, this);
  UpdateSitePermissionsPage(site_permissions_page.get());

  SwitchToPage(std::move(site_permissions_page));

  base::RecordAction(
      base::UserMetricsAction("Extensions.Menu.SitePermissionsPageOpened"));
}

void ExtensionsMenuDelegateDesktop::CloseBubble() {
  bubble_contents_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void ExtensionsMenuDelegateDesktop::OnSiteAccessSelected(
    const extensions::ExtensionId& extension_id,
    PermissionsManager::UserSiteAccess site_access) {
  menu_model_->UpdateSiteAccess(extension_id, site_access);
}

void ExtensionsMenuDelegateDesktop::OnActionButtonClicked(
    const extensions::ExtensionId& extension_id) {
  menu_model_->ExecuteAction(extension_id);
}

void ExtensionsMenuDelegateDesktop::OnSiteSettingsToggleButtonPressed(
    bool is_on) {
  PermissionsManager::UserSiteSetting site_setting =
      is_on ? PermissionsManager::UserSiteSetting::kCustomizeByExtension
            : PermissionsManager::UserSiteSetting::kBlockAllExtensions;
  menu_model_->UpdateSiteSetting(site_setting);
}

void ExtensionsMenuDelegateDesktop::OnExtensionToggleSelected(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  if (is_on) {
    menu_model_->GrantSiteAccess(extension_id);
  } else {
    menu_model_->RevokeSiteAccess(extension_id);
  }
}

void ExtensionsMenuDelegateDesktop::OnReloadPageButtonClicked() {
  menu_model_->ReloadWebContents();
}

void ExtensionsMenuDelegateDesktop::OnAllowExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  menu_model_->AllowHostAccessRequest(extension_id);
}

void ExtensionsMenuDelegateDesktop::OnDismissExtensionClicked(
    const extensions::ExtensionId& extension_id) {
  menu_model_->DismissHostAccessRequest(extension_id);
}

void ExtensionsMenuDelegateDesktop::OnShowRequestsTogglePressed(
    const extensions::ExtensionId& extension_id,
    bool is_on) {
  menu_model_->ShowHostAccessRequestsInToolbar(extension_id, is_on);
}

void ExtensionsMenuDelegateDesktop::UpdateMainPage(
    ExtensionsMenuMainPageView* main_page) {
  // Update site settings.
  ExtensionsMenuViewModel::SiteSettingsState site_settings_state =
      menu_model_->GetSiteSettingsState();
  main_page->UpdateSiteSettings(site_settings_state);

  // Update the optional section.
  ExtensionsMenuViewModel::OptionalSection optional_section =
      menu_model_->GetOptionalSection();
  if (optional_section ==
      ExtensionsMenuViewModel::OptionalSection::kHostAccessRequests) {
    // Clear any existing requests first to ensure a clean state.
    // Note: There are few times when we will be re-populating the requests
    // unnecessarily. This is okay because it should be uncommon, as it requires
    // various extensions to add a host access request for a specific site and
    // the user to have withheld site access for such extensions.
    main_page->ClearExtensionsRequestingAccess();

    const auto& requests = menu_model_->host_access_requests();
    for (size_t i = 0; i < requests.size(); ++i) {
      ExtensionsMenuViewModel::HostAccessRequest request =
          GetHostAccessRequest(*menu_model_, requests[i]);
      main_page->AddExtensionRequestingAccess(request, i);
    }
  }
  main_page->SetOptionalSectionVisibility(optional_section);

  // Update menu entries.
  // TODO(crbug.com/40879945): Reorder the extensions after updating them, since
  // their names can change.
  std::vector<ExtensionsMenuEntryView*> menu_entries =
      main_page->GetMenuEntries();
  gfx::Size icon_size = GetMenuExtensionIconSize();
  for (auto* menu_entry : menu_entries) {
    ExtensionsMenuViewModel::MenuEntryState entry_state =
        menu_model_->GetMenuEntryState(menu_entry->extension_id(), icon_size);
    menu_entry->Update(entry_state);
  }
}

void ExtensionsMenuDelegateDesktop::UpdateSitePermissionsPage(
    ExtensionsMenuSitePermissionsPageView* site_permissions_page) {
  extensions::ExtensionId extension_id = site_permissions_page->extension_id();
  gfx::Size icon_size = GetMenuExtensionIconSize();

  ExtensionsMenuViewModel::ExtensionSitePermissionsState
      site_permissions_state = menu_model_->GetExtensionSitePermissionsState(
          extension_id, icon_size);
  site_permissions_page->Update(site_permissions_state);
}

ExtensionsMenuMainPageView*
ExtensionsMenuDelegateDesktop::GetMainPageViewForTesting() {
  DCHECK(current_page_);
  return GetMainPage(current_page_.view());
}

ExtensionsMenuSitePermissionsPageView*
ExtensionsMenuDelegateDesktop::GetSitePermissionsPageForTesting() {
  DCHECK(current_page_);
  return GetSitePermissionsPage(current_page_.view());
}

void ExtensionsMenuDelegateDesktop::SwitchToPage(
    std::unique_ptr<views::View> page) {
  if (current_page_) {
    bubble_contents_->RemoveChildViewT(current_page_.view());
  }
  DCHECK(!current_page_);
  current_page_.SetView(bubble_contents_->AddChildView(std::move(page)));
}

void ExtensionsMenuDelegateDesktop::PopulateMainPage(
    ExtensionsMenuMainPageView* main_page) {
  const std::vector<std::unique_ptr<ExtensionActionViewModel>>& action_models =
      menu_model_->action_models();
  for (size_t i = 0; i < action_models.size(); ++i) {
    InsertMenuEntry(main_page, action_models[i].get(), i);
  }
}

void ExtensionsMenuDelegateDesktop::InsertMenuEntry(
    ExtensionsMenuMainPageView* main_page,
    ExtensionActionViewModel* action_model,
    int index) {
  gfx::Size icon_size = GetMenuExtensionIconSize();
  ExtensionsMenuViewModel::MenuEntryState entry_state =
      menu_model_->GetMenuEntryState(action_model->GetId(), icon_size);
  main_page->CreateAndInsertMenuEntry(action_model, entry_state, index);
}
