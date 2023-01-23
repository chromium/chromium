// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "extensions/browser/permissions_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
class TabbedPane;
}  // namespace views

class InstalledExtensionMenuItemView;
class SiteAccessMenuItemView;
class ExtensionsContainer;
class SiteSettingsExpandButton;

// ExtensionsTabbedMenuView is the extensions menu dialog with a tabbed pane.
// TODO(crbug.com/1263311): Brief explanation of each tabs goal after
// implementing them.
class ExtensionsTabbedMenuView
    : public views::BubbleDialogDelegateView,
      public TabStripModelObserver,
      public ToolbarActionsModel::Observer,
      public extensions::PermissionsManager::Observer {
 public:
  METADATA_HEADER(ExtensionsTabbedMenuView);
  ExtensionsTabbedMenuView(views::View* anchor_view,
                           Browser* browser,
                           ExtensionsContainer* extensions_container,
                           bool allow_pinning);
  ~ExtensionsTabbedMenuView() override;
  ExtensionsTabbedMenuView(const ExtensionsTabbedMenuView&) = delete;
  const ExtensionsTabbedMenuView& operator=(const ExtensionsTabbedMenuView&) =
      delete;

  // Returns the currently-showing extension items in the extensions tab, if any
  // exists.
  std::vector<InstalledExtensionMenuItemView*> GetInstalledItemsForTesting()
      const;

  // Returns the currently-showing `has_access_` extension items in the site
  // access tab, if any exists.
  std::vector<SiteAccessMenuItemView*> GetVisibleHasAccessItemsForTesting()
      const;

  // Returns the currently-showing `requests_access_` extension items in the
  // site access tab, if any exists.
  std::vector<SiteAccessMenuItemView*> GetVisibleRequestsAccessItemsForTesting()
      const;

  // Returns the site access message view in the site access tab.
  views::Label* GetSiteAccessMessageForTesting() const;

  // Returns the `discover_more_button_` in the extensions tab.
  HoverButton* GetDiscoverMoreButtonForTesting() const;

  // Returns the `site_settings_button_` in the site access tab.
  HoverButton* GetSiteSettingsButtonForTesting() const;

  // Returns the `site_settings_` in the site access tab.
  views::View* GetSiteSettingsForTesting() const;

  // Returns the index of the currently selected tab.
  size_t GetSelectedTabIndex() const;

  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // PermissionsManager::Observer:
  void OnUserPermissionsSettingsChanged(
      const extensions::PermissionsManager::UserPermissionsSettings& settings)
      override;

 private:
  struct SiteAccessSection {
    // The root view for this section used to toggle the visibility of the
    // entire section (depending on whether there are any menu items).
    raw_ptr<views::View> container;

    // The view containing the section heder. The text changes based on the
    // current site.
    raw_ptr<views::Label> header;

    // The view containing only the menu items for this section.
    raw_ptr<views::View> items;

    // The id of the string to use for the section heading. Does not include the
    // current site string.
    const int header_string_id;
  };

  // Initially creates the tabs.
  void Populate();

  // Updates the menu.
  void Update();

  void CreateSiteAccessTab();
  void CreateExtensionsTab();

  // Creates and adds a menu item for `id` in the installed extensions for a
  // newly-added extension.
  void CreateAndInsertInstalledExtension(
      const ToolbarActionsModel::ActionId& id,
      size_t index);

  // Creates and adds a menu item for `id` in its corresponding site access
  // section if the associated extension has or requests access to the current
  // site.
  void MaybeCreateAndInsertSiteAccessItem(
      const ToolbarActionsModel::ActionId& id);

  // Adds `item` to the items list of `section`.
  void InsertSiteAccessItem(std::unique_ptr<SiteAccessMenuItemView> item,
                            SiteAccessSection* section);

  // Updates the installed extension menu items corresponding to `action_ids`,
  // and their positions in the extensions tab.
  void UpdateInstalledExtensionMenuItems(
      const base::flat_set<ToolbarActionsModel::ActionId>& action_ids);

  // Updates the site access menu items corresponding to `action_ids`, and their
  // positions in their corresponding site access section (moving the item
  // between sections if necessary). Note that if there is no site access item
  // for an action id, it creates and inserts a site access item in its
  // corresponding site access section.
  void UpdateSiteAccessMenuItems(
      const base::flat_set<ToolbarActionsModel::ActionId>& action_ids);

  // Updates the visibility and contents of the site access tab based on the
  // current site setting.
  void UpdateSiteAccessTab();

  // Updates the visibility and header of the site access sections, and whether
  // their items should `show_combobox`. A given section should be visible if
  // there are any extensions displayed in it.
  void UpdateSiteAccessSectionsVisibility(bool show_combobox);

  // Returns the section corresponding to `site_interaction`, or nullptr.
  SiteAccessSection* GetSectionForAction(ToolbarActionViewController* action);

  // Returns the currently-showing menu items for `section` in the
  // site access tab, if any exists.
  std::vector<SiteAccessMenuItemView*> GetVisibleMenuItemsOf(
      SiteAccessSection section) const;

  // Returns the current web contents in `browser_`.
  content::WebContents* GetActiveWebContents();

  // Handles the selection of a site setting radio button.
  void OnSiteSettingSelected(
      extensions::PermissionsManager::UserSiteSetting site_setting);

  // Shows or hides the site setting options when `site_settings_button_` is
  // pressed.
  void OnSiteSettingsButtonPressed();

  // Runs a set of consistency checks on the appearance of the menu. This is a
  // no-op if DCHECKs are disabled.
  void ConsistencyCheck();

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsContainer> extensions_container_;
  const raw_ptr<ToolbarActionsModel> toolbar_model_;
  const bool allow_pinning_;

  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};
  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};

  // The view containing the menu's two tabs.
  raw_ptr<views::TabbedPane> tabbed_pane_ = nullptr;

  // The view containing the installed menu items in the extensions tab. This is
  // separated for easy insertion and iteration of menu items. The children are
  // guaranteed to only be ExtensionMenuItemViews.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* installed_items_ = nullptr;

  // The button used to open the webstore page in the extensions tab.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION HoverButton* discover_more_button_ = nullptr;

  // The view containing a message in the site access tab.
  raw_ptr<views::Label> site_access_message_ = nullptr;

  // The button used to open the site settings in the site access tab.
  raw_ptr<SiteSettingsExpandButton> site_settings_button_ = nullptr;

  // The view containing the site settings in the site access tab.
  raw_ptr<views::View> site_settings_ = nullptr;

  // The visibility of the site settings view. By default, it is not visible.
  bool show_site_settings_ = false;

  // The different sections in the site access tab.
  SiteAccessSection requests_access_;
  SiteAccessSection has_access_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionsTabbedMenuView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsTabbedMenuView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_
