// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"

class Browser;
class InstalledExtensionMenuItemView;
class ExtensionsMenuView;
class ExtensionsToolbarContainer;

// An implementation of ExtensionActionTestHelper that works with the
// ExtensionsMenu.
class ExtensionsMenuTestUtil : public ExtensionActionTestHelper {
 public:
  ExtensionsMenuTestUtil(Browser* browser, bool is_real_window);
  ExtensionsMenuTestUtil(const ExtensionsMenuTestUtil&) = delete;
  ExtensionsMenuTestUtil& operator=(const ExtensionsMenuTestUtil&) = delete;
  ~ExtensionsMenuTestUtil() override;

  // ExtensionActionTestHelper:
  int NumberOfBrowserActions() override;
  int VisibleBrowserActions() override;
  bool HasAction(const extensions::ExtensionId& id) override;
  void InspectPopup(const extensions::ExtensionId& id) override;
  bool HasIcon(const extensions::ExtensionId& id) override;
  gfx::Image GetIcon(const extensions::ExtensionId& id) override;
  void Press(const extensions::ExtensionId& id) override;
  std::string GetTooltip(const extensions::ExtensionId& id) override;
  gfx::NativeView GetPopupNativeView() override;
  bool HasPopup() override;
  bool HidePopup() override;
  ExtensionsContainer* GetExtensionsContainer() override;
  void WaitForExtensionsContainerLayout() override;
  gfx::Size GetMinPopupSize() override;
  gfx::Size GetMaxPopupSize() override;
  gfx::Size GetToolbarActionSize() override;
  gfx::Size GetMaxAvailableSizeToFitBubbleOnScreen(
      const extensions::ExtensionId& id) override;

 private:
  class MenuViewObserver;
  class Wrapper;

  // Returns the InstalledExtensionMenuItemView for the given `id` from the
  // `menu_view`.
  InstalledExtensionMenuItemView* GetMenuItemViewForId(
      const extensions::ExtensionId& id);

  // An override to allow test instances of the ExtensionsMenuView.
  // This has to be defined before |menu_view_| below.
  base::AutoReset<bool> scoped_allow_extensions_menu_instances_;

  std::unique_ptr<Wrapper> wrapper_;

  const raw_ptr<Browser, DanglingUntriaged> browser_;
  raw_ptr<ExtensionsToolbarContainer, DanglingUntriaged> extensions_container_ =
      nullptr;

  // Helps make sure that `menu_view_`, if existent, is set to null when
  // destroyed by the widget or via manual means.
  std::unique_ptr<MenuViewObserver> menu_view_observer_;

  // The actual pointer to an ExtensionsMenuView, non-null if alive.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION ExtensionsMenuView* menu_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
