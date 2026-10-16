// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "ui/views/view_observer.h"

class BrowserWindowInterface;
class ExtensionsContainer;
class ExtensionsContainerViews;
class ExtensionsMenuCoordinator;
class ExtensionsMenuView;
class ExtensionsToolbarDesktop;
class HoverButton;
class ToolbarActionViewModel;
class WebUIToolbarExtensionsContainer;

// An implementation of ExtensionActionTestHelper that works with the
// ExtensionsMenu.
//
// The extensions menu itself is always a Views bubble, so most of this class
// operates on the platform-abstract ExtensionsContainer and
// ExtensionsContainerViews interfaces. The toolbar container hosting the
// extension actions can either be the Views-based ExtensionsToolbarDesktop
// or, when features::IsWebUIExtensionsContainerEnabled(), the
// WebUIToolbarExtensionsContainer backing the WebUI toolbar. Only the few
// operations that need to look at the toolbar's actual buttons branch on which
// of the two is in use.
class ExtensionsMenuTestUtil : public ExtensionActionTestHelper,
                               views::ViewObserver {
 public:
  explicit ExtensionsMenuTestUtil(BrowserWindowInterface* browser);
  ExtensionsMenuTestUtil(const ExtensionsMenuTestUtil&) = delete;
  ExtensionsMenuTestUtil& operator=(const ExtensionsMenuTestUtil&) = delete;
  ~ExtensionsMenuTestUtil() override;

  // ExtensionActionTestHelper:
  int NumberOfBrowserActions() override;
  bool HasAction(const extensions::ExtensionId& id) override;
  void InspectPopup(const extensions::ExtensionId& id) override;
  gfx::Image GetIcon(const extensions::ExtensionId& id) override;
  void Press(const extensions::ExtensionId& id) override;
  gfx::NativeView GetPopupNativeView() override;
  bool HasPopup() override;
  bool HidePopup() override;
  void WaitForExtensionsContainerLayout() override;
  gfx::Size GetMinPopupSize() override;
  gfx::Size GetMaxPopupSize() override;
  gfx::Size GetMaxAvailableSizeToFitBubbleOnScreen(
      const extensions::ExtensionId& id) override;

  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  // Opens the extensions menu if it's not already showing.
  void OpenExtensionsMenu();

  // Returns whether the extensions menu created by this test helper is showing.
  bool IsExtensionsMenuShowing();

  // Returns the action button for the given `id` from the `menu_view_`.
  HoverButton* GetActionButton(const extensions::ExtensionId& id);

  // Returns the ExtensionsMenuCoordinator of whichever container is in use.
  // Only non-null when extensions_features::kExtensionsMenuAccessControl is
  // enabled.
  ExtensionsMenuCoordinator* GetExtensionsMenuCoordinator();

  // Returns the action that owns the currently showing popup, if any.
  ToolbarActionViewModel* GetPopupOwner();

  // An override to allow test instances of the ExtensionsMenuView.
  // This has to be defined before |menu_view_| below.
  base::AutoReset<bool> scoped_allow_extensions_menu_instances_;

  const raw_ptr<BrowserWindowInterface, DanglingUntriaged> browser_;

  // Exactly one of these is non-null, depending on whether the WebUI
  // extensions container is enabled.
  raw_ptr<ExtensionsToolbarDesktop, DanglingUntriaged> extensions_toolbar_ =
      nullptr;
  raw_ptr<WebUIToolbarExtensionsContainer, DanglingUntriaged>
      webui_extensions_container_ = nullptr;

  // The platform-abstract interfaces of whichever container is in use.
  raw_ptr<ExtensionsContainer, DanglingUntriaged> extensions_container_ =
      nullptr;
  raw_ptr<ExtensionsContainerViews, DanglingUntriaged>
      extensions_container_views_ = nullptr;

  // The actual pointer to an ExtensionsMenuView, non-null if alive.
  raw_ptr<ExtensionsMenuView> menu_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_TEST_UTIL_H_
