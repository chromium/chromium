// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ToggleButton;
}  // namespace views

class Browser;
class ExtensionsMenuHandler;
class ExtensionActionViewModel;

// The main view of the extensions menu.
class ExtensionsMenuMainPageView : public views::View {
  METADATA_HEADER(ExtensionsMenuMainPageView, views::View)

 public:
  explicit ExtensionsMenuMainPageView(Browser* browser,
                                      ExtensionsMenuHandler* menu_handler);
  ~ExtensionsMenuMainPageView() override;
  ExtensionsMenuMainPageView(const ExtensionsMenuMainPageView&) = delete;
  const ExtensionsMenuMainPageView& operator=(
      const ExtensionsMenuMainPageView&) = delete;

  // Creates and adds a menu entry for `action_model` with `entry_state` at
  // `index` for a newly-added extension.
  void CreateAndInsertMenuEntry(
      ExtensionActionViewModel* action_model,
      ExtensionsMenuViewModel::MenuEntryState entry_state,
      int index);

  // Removes the menu entry at `index`.
  void RemoveMenuEntry(int index);

  // Returns the menu entry views.
  std::vector<ExtensionsMenuEntryView*> GetMenuEntries() const;

  // Updates the site settings views with the given parameters.
  void UpdateSiteSettings(
      ExtensionsMenuViewModel::SiteSettingsState site_settings_state);

  // Adds or updates the extension entry in the `requests_access_section_` at
  // `index` with the given information. Doesn't update the requests section
  // view visibility.
  void AddExtensionRequestingAccess(
      ExtensionsMenuViewModel::HostAccessRequest request,
      int index);

  // Updates the extension entry in the `requests_access_section_` at `index`
  // with the given information. Doesn't update the requests section view
  // visibility.
  void UpdateExtensionRequestingAccess(
      ExtensionsMenuViewModel::HostAccessRequest request,
      int index);

  // Removes the entry in the `requests_access_section_` at `index`. Doesn't
  // update the requests section view visibility.
  void RemoveExtensionRequestingAccess(const extensions::ExtensionId& id,
                                       int index);

  // Clears the entries in the `request_access_section_`, if existent. Doesn't
  // update the requests section view visibility.
  void ClearExtensionsRequestingAccess();

  // Shows/hides the optional section.
  void SetOptionalSectionVisibility(
      ExtensionsMenuViewModel::OptionalSection optional_section);

  // Accessors used by tests:
  std::u16string_view GetSiteSettingLabelForTesting() const;
  const views::View* site_settings_tooltip() const;
  views::ToggleButton* GetSiteSettingsToggleForTesting() {
    return site_settings_toggle_;
  }
  const views::View* reload_section() const;
  const views::View* requests_section() const;
  std::vector<extensions::ExtensionId>
  GetExtensionsRequestingAccessForTesting();
  views::View* GetExtensionRequestingAccessEntryForTesting(
      const extensions::ExtensionId& extension_id);

 private:
  // Returns the header builder, which contains information about the site.
  [[nodiscard]] views::Builder<views::FlexLayoutView> CreateHeaderBuilder(
      gfx::Insets margins,
      views::FlexSpecification stretch_specification);

  // Returns the site settings section builder, which contains information and
  // access controls for the site.
  [[nodiscard]] views::Builder<views::FlexLayoutView> CreateSiteSettingsBuilder(
      gfx::Insets margins,
      views::FlexSpecification);

  // Returns the contents builder, which contains the reload section, the access
  // requests section and the menu entries section on a scrollable view.
  [[nodiscard]] views::Builder<views::ScrollView> CreateContentsBuilder(
      gfx::Insets scroll_margins,
      gfx::Insets contents_margins,
      gfx::Insets reload_button_margins,
      gfx::Insets menu_entries_margins);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Returns the webstore button builder.
  [[nodiscard]] views::Builder<HoverButton> CreateWebstoreButtonBuilder();
#endif

  // Returns the manage extensions button builder.
  [[nodiscard]] views::Builder<HoverButton> CreateManageButtonBuilder();

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsMenuHandler> menu_handler_;

  // Site settings section.
  raw_ptr<views::Label> site_settings_label_;
  raw_ptr<views::View> site_settings_tooltip_;
  raw_ptr<views::ToggleButton> site_settings_toggle_;

  // Reload section.
  raw_ptr<views::View> reload_section_;

  // Site access requests section.
  raw_ptr<views::View> requests_section_;
  // View that holds the requests entries in `requests_section_`.
  raw_ptr<views::View> requests_entries_view_;

  // Menu entries section. The children are guaranteed to only be
  // ExtensionsMenuEntryView.
  raw_ptr<views::View> menu_entries_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
