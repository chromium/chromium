// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"

#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns sorted extension ids based on their extensions name.
std::vector<std::string> SortExtensionsByName(
    ToolbarActionsModel* toolbar_model) {
  auto sort_by_name = [toolbar_model](const ToolbarActionsModel::ActionId a,
                                      const ToolbarActionsModel::ActionId b) {
    return base::i18n::ToLower(toolbar_model->GetExtensionName(a)) <
           base::i18n::ToLower(toolbar_model->GetExtensionName(b));
  };
  std::vector<std::string> sorted_ids(toolbar_model->action_ids().begin(),
                                      toolbar_model->action_ids().end());
  std::sort(sorted_ids.begin(), sorted_ids.end(), sort_by_name);
  return sorted_ids;
}

// Returns the index of `action_id` in the toolbar model actions based on the
// extensions name alphabetical order.
size_t FindIndex(ToolbarActionsModel* toolbar_model,
                 const ToolbarActionsModel::ActionId action_id) {
  std::u16string extension_name =
      base::i18n::ToLower(toolbar_model->GetExtensionName(action_id));
  auto sorted_action_ids = SortExtensionsByName(toolbar_model);
  return static_cast<size_t>(
      base::ranges::lower_bound(sorted_action_ids, extension_name, {},
                                [toolbar_model](std::string id) {
                                  return base::i18n::ToLower(
                                      toolbar_model->GetExtensionName(id));
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
}

ExtensionsMenuViewController::~ExtensionsMenuViewController() {
  // Note: No need to call TabStripModel::RemoveObserver(), because it's handled
  // directly within TabStripModelObserver::~TabStripModelObserver().
}

void ExtensionsMenuViewController::OpenMainPage() {
  auto main_page = std::make_unique<ExtensionsMenuMainPageView>(browser_, this);
  PopulateMainPage(main_page.get());

  SwitchToPage(std::move(main_page));
}

void ExtensionsMenuViewController::OpenSitePermissionsPage(
    extensions::ExtensionId extension_id) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  std::unique_ptr<ExtensionActionViewController> action_controller =
      ExtensionActionViewController::Create(extension_id, browser_,
                                            extensions_container_);

  std::u16string extension_name = action_controller->GetActionName();
  ui::ImageModel extension_icon = action_controller->GetIcon(
      GetActiveWebContents(), gfx::Size(icon_size, icon_size));

  auto site_permissions_page =
      std::make_unique<ExtensionsMenuSitePermissionsPageView>(
          browser_, extension_name, extension_icon, extension_id, this);
  SwitchToPage(std::move(site_permissions_page));
}

void ExtensionsMenuViewController::CloseBubble() {
  bubble_contents_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
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

  ExtensionsMenuMainPageView* main_page = GetMainPage(current_page_);
  if (main_page && web_contents) {
    main_page->Update(web_contents);
  }
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

  int index = FindIndex(toolbar_model_, action_id);
  std::unique_ptr<ExtensionActionViewController> action_controller =
      ExtensionActionViewController::Create(action_id, browser_,
                                            extensions_container_);

  main_page->CreateAndInsertMenuItem(
      std::move(action_controller), action_id,
      extensions_container_->CanShowActionsInToolbar(), index);

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
  if (GetSitePermissionsPage(current_page_)) {
    NOTREACHED();
    return;
  }

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
  main_page->UpdatePinButtons();
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
  current_page_ = bubble_contents_->AddChildView(std::move(page));

  // Only resize the menu if the bubble is created, since page could be added to
  // the menu beforehand and delegate wouldn't know the bubble bounds.
  if (bubble_delegate_->GetBubbleFrameView()) {
    bubble_delegate_->SizeToContents();
  }
}

void ExtensionsMenuViewController::PopulateMainPage(
    ExtensionsMenuMainPageView* main_page) {
  bool allow_pinning = extensions_container_->CanShowActionsInToolbar();
  std::vector<std::string> sorted_ids = SortExtensionsByName(toolbar_model_);
  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    // TODO(emiliapaz): Under MVC architecture, view should not own the view
    // controller. However, the current extensions structure depends on this
    // thus a major restructure is needed.
    std::unique_ptr<ExtensionActionViewController> action_controller =
        ExtensionActionViewController::Create(sorted_ids[i], browser_,
                                              extensions_container_);
    main_page->CreateAndInsertMenuItem(std::move(action_controller),
                                       sorted_ids[i], allow_pinning, i);
  }
}

content::WebContents* ExtensionsMenuViewController::GetActiveWebContents()
    const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
