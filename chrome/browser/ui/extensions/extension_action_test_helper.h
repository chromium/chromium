// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "extensions/common/extension_id.h"
#include "ui/gfx/native_widget_types.h"

class Browser;

namespace gfx {
class Image;
class Size;
}  // namespace gfx

// TODO(crbug.com/40177062): A lot of this class can be cleaned up for
// the new toolbar UI. Some of it may also be removable, since we now have
// the platform-abstract ExtensionsContainer class.
class ExtensionActionTestHelper {
 public:
  // Constructs a ExtensionActionTestHelper which, if |is_real_window| is false,
  // will create its own browser actions container. This is useful in unit
  // tests, when the |browser|'s window doesn't create platform-specific views.
  static std::unique_ptr<ExtensionActionTestHelper> Create(
      Browser* browser,
      bool is_real_window = true);

  ExtensionActionTestHelper(const ExtensionActionTestHelper&) = delete;
  ExtensionActionTestHelper& operator=(const ExtensionActionTestHelper&) =
      delete;

  virtual ~ExtensionActionTestHelper() {}

  // Returns the number of browser action buttons in the window toolbar.
  virtual int NumberOfBrowserActions() = 0;

  // Returns true if there is an action for the given `id`.
  virtual bool HasAction(const extensions::ExtensionId& id) = 0;

  // Simulates a user click on the action button for the given `id`.
  virtual void Press(const extensions::ExtensionId& id) = 0;

  // Inspects the extension popup for the action with the given `id`.
  virtual void InspectPopup(const extensions::ExtensionId& id) = 0;

  // Trigger an extension popup as a result of API.
  virtual void TriggerPopupForAPI(const extensions::ExtensionId& id) = 0;

  // Returns icon for the action for the given `id`.
  virtual gfx::Image GetIcon(const extensions::ExtensionId& id) = 0;

  virtual gfx::NativeView GetPopupNativeView() = 0;

  // Spins a RunLoop until the NativeWindow hosting |GetPopupNativeView()| is
  // reported as active by the OS. This method is strange: it's not overridden
  // by subclasses, and instead the implementation is selected at compile-time
  // depending on the windowing system in use.
  void WaitForPopup();

  // Returns whether a browser action popup is being shown currently.
  virtual bool HasPopup() = 0;

  // Hides the given popup and returns whether the hide was successful.
  virtual bool HidePopup() = 0;

  // Waits for the ExtensionContainer's layout to be done.
  virtual void WaitForExtensionsContainerLayout() = 0;

  // Returns the minimum allowed size of an extension popup.
  virtual gfx::Size GetMinPopupSize() = 0;

  // Returns the maximum allowed size of an extension popup.
  virtual gfx::Size GetMaxPopupSize() = 0;

  // Returns the maximum available size to place a bubble anchored to
  // the action with the given `id` on screen.
  virtual gfx::Size GetMaxAvailableSizeToFitBubbleOnScreen(
      const extensions::ExtensionId& id) = 0;

 protected:
  ExtensionActionTestHelper() {}
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ACTION_TEST_HELPER_H_
