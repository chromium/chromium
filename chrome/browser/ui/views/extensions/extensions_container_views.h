// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_CONTAINER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_CONTAINER_VIEWS_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "extensions/common/extension_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class ToolbarActionViewModel;

namespace views {
class FocusManager;
class Widget;
}  // namespace views

// An interface with views-specific methods for the toolbar.
//
// While ExtensionsContainer provides a minimal set of APIs for non-UI code,
// this interface provides methods for views-specific extension UI subcomponents
// to interact with the extension toolbar.
class ExtensionsContainerViews {
 public:
  // Returns true if the container itself is visible.
  virtual bool IsVisible() const = 0;

  // Get the currently popped out action id, if any.
  // TODO(pbos): Consider supporting multiple popped out actions for bubbles
  // that relate to more than one extension.
  virtual std::optional<extensions::ExtensionId> GetPoppedOutActionId()
      const = 0;

  // Returns true if the action pointed by `action_id` is visible on the
  // toolbar.
  virtual bool IsActionVisibleOnToolbar(const std::string& action_id) const = 0;

  // Undoes the current "pop out"; i.e., moves the popped out action back into
  // overflow.
  virtual void UndoPopOut() = 0;

  // Sets the active popup owner to be |popup_owner|.
  virtual void SetPopupOwner(ToolbarActionViewModel* popup_owner) = 0;

  // Pops out `action_id`, ensuring it is visible. `closure` will be called once
  // any animation is complete.
  virtual void PopOutAction(const extensions::ExtensionId& action_id,
                            base::OnceClosure closure) = 0;

  // Pop out and show the extension button corresponding to `extension_id`, then
  // anchor `widget` to it and show `widget`. If the extension button is already
  // visible, the anchoring and showing may happen immediately/synchronously.
  virtual void ShowWidgetForExtension(views::Widget* widget,
                                      const std::string& extension_id) = 0;

  // Collapses the confirmation on the request access button, effectively
  // hiding the button. Does nothing if the confirmation is not showing
  // anymore.
  virtual void CollapseConfirmation() = 0;

  // Shows the context menu for the action as a fallback for performing another
  // action.
  virtual void ShowContextMenuAsFallback(
      const extensions::ExtensionId& action_id) = 0;

  // Called when a popup is shown. If `by_user` is true, then this was through
  // a direct user action (as opposed to, e.g., an API call).
  virtual void OnPopupShown(const extensions::ExtensionId& action_id,
                            bool by_user) = 0;

  // Called when a popup is closed.
  virtual void OnPopupClosed(const extensions::ExtensionId& action_id) = 0;

  // Returns the FocusManager to use when registering accelerators.
  virtual views::FocusManager* GetFocusManagerForAccelerator() = 0;

  // Returns the reference button for the extension action's popup. Rather than
  // relying on the button being a MenuButton, the button returned should have a
  // MenuButtonController. This is part of the ongoing work from
  // http://crbug.com/41423998 to simplify the button hierarchy by migrating
  // controller logic into a separate class leaving MenuButton as an empty class
  // to be deprecated. Can fall back to GetExtensionsButtonAnchor().
  virtual views::BubbleAnchor GetReferenceButtonForPopup(
      const extensions::ExtensionId& action_id) = 0;

  // Returns an anchor to the extensions button (puzzle piece). It can fall back
  // to an anchor to the parent window; this allows a Widget to be created with
  // this anchor and later re-anchored (e.g. via ShowWidgetForExtension()) to
  // this extensions container, as a Widget's parent window is set from its
  // anchor at creation time and cannot be changed during re-anchoring.
  virtual views::BubbleAnchor GetExtensionsButtonAnchor() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_CONTAINER_VIEWS_H_
