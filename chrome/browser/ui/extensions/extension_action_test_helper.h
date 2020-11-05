// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class ExtensionsContainer;
class ToolbarActionsBar;

namespace gfx {
class Image;
class Size;
}  // namespace gfx

class ExtensionActionTestHelper {
 public:
  // Constructs a ExtensionActionTestHelper which, if |is_real_window| is false,
  // will create its own browser actions container. This is useful in unit
  // tests, when the |browser|'s window doesn't create platform-specific views.
  static std::unique_ptr<ExtensionActionTestHelper> Create(
      Browser* browser,
      bool is_real_window = true);

  virtual ~ExtensionActionTestHelper() {}

  // Returns the number of browser action buttons in the window toolbar.
  virtual int NumberOfBrowserActions() = 0;

  // Returns the number of browser action currently visible. Note that a correct
  // result may require a UI layout. Ensure the UI layout is up-to-date (e.g. by
  // calling InProcessBrowserTest::RunScheduledLayouts()) for a browser test.
  virtual int VisibleBrowserActions() = 0;

  // Inspects the extension popup for the action at the given index.
  virtual void InspectPopup(int index) = 0;

  // Returns whether the brownser action at |index| has a non-null icon. Note
  // that the icon is loaded asynchronously, in which case you can wait for it
  // to load by calling WaitForBrowserActionUpdated.
  virtual bool HasIcon(int index) = 0;

  // Returns icon for the browser action at |index|.
  virtual gfx::Image GetIcon(int index) = 0;

  // Simulates a user click on the browser action button at |index|.
  virtual void Press(int index) = 0;

  // Returns the extension id of the extension at |index|.
  virtual std::string GetExtensionId(int index) = 0;

  // Returns the current tooltip for the browser action button.
  virtual std::string GetTooltip(int index) = 0;

  virtual gfx::NativeView GetPopupNativeView() = 0;

  // Spins a RunLoop until the NativeWindow hosting |GetPopupNativeView()| is
  // reported as active by the OS. Returns true if successful. This method is
  // strange: it's not overridden by subclasses, and instead the implementation
  // is selected at compile-time depending on the windowing system in use.
  bool WaitForPopup();

  // Returns whether a browser action popup is being shown currently.
  virtual bool HasPopup() = 0;

  // Hides the given popup and returns whether the hide was successful.
  virtual bool HidePopup() = 0;

  // Sets the current width of the browser actions container without resizing
  // the underlying controller. This is to simulate e.g. when the browser window
  // is too small for the preferred width.
  virtual void SetWidth(int width) = 0;

  // Returns the ToolbarActionsBar.
  virtual ToolbarActionsBar* GetToolbarActionsBar() = 0;

  // Returns the associated ExtensionsContainer.
  virtual ExtensionsContainer* GetExtensionsContainer() = 0;

  // Waits for the ExtensionContainer's layout to be done.
  virtual void WaitForExtensionsContainerLayout() = 0;

  // Creates and returns a ExtensionActionTestHelper with an "overflow"
  // container, with this object's container as the main bar.
  virtual std::unique_ptr<ExtensionActionTestHelper> CreateOverflowBar(
      Browser* browser) = 0;

  // Forces a layout of an overflow bar. Must only be called on the helper
  // returned by CreateOverflowBar().
  virtual void LayoutForOverflowBar() = 0;

  // Returns the minimum allowed size of an extension popup.
  virtual gfx::Size GetMinPopupSize() = 0;

  // Returns the size of the toolbar actions.
  virtual gfx::Size GetToolbarActionSize() = 0;

  // Returns the maximum allowed size of an extension popup.
  virtual gfx::Size GetMaxPopupSize() = 0;

  // Returns the maximum available size to place a bubble anchored to
  // the browser action at |action_index| on screen.
  virtual gfx::Size GetMaxAvailableSizeToFitBubbleOnScreen(
      int action_index) = 0;

 protected:
  ExtensionActionTestHelper() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionActionTestHelper);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_H_
