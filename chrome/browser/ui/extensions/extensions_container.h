// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_

#include <string>

#include "chrome/browser/ui/extensions/extension_popup_types.h"

class ToolbarActionViewModel;

// An interface for containers in the toolbar that host extensions.
//
// This interface provides a minimal set of APIs that allows non-UI code to
// interact with the extension toolbar UI. Add new methods to this interface
// only if they are called from non-UI code.
class ExtensionsContainer {
 public:
  // Returns the action for the given |id|, if one exists.
  virtual ToolbarActionViewModel* GetActionForId(
      const std::string& action_id) = 0;

  // Hides the actively showing popup, if any.
  virtual void HideActivePopup() = 0;

  // Closes the overflow menu, if it was open. Returns whether or not the
  // overflow menu was closed.
  virtual bool CloseOverflowMenuIfOpen() = 0;

  // Shows the popup for the action with |id| as the result of an API call,
  // returning true if a popup is shown and invoking |callback| upon completion.
  virtual bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                                ShowPopupCallback callback) = 0;

  // Toggle the Extensions menu (as if the user clicked the puzzle piece icon).
  virtual void ToggleExtensionsMenu() = 0;

  // Whether there are any Extensions registered with the ExtensionsContainer.
  virtual bool HasAnyExtensions() const = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_
