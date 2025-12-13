// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_

#include <optional>

#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class AppMenuButton;
class AvatarToolbarButton;
class PinnedToolbarActionsContainer;
class ExtensionsToolbarContainer;
class IconLabelBubbleView;
class IntentChipButton;
class PageActionIconView;
class ReloadButton;
class ReloadControl;
class ToolbarButton;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class AccessiblePaneView;
class View;
}  // namespace views

// An interface implemented by a view contains and provides access to toolbar
// buttons in a BrowserView.
class ToolbarButtonProvider {
 public:
  // Gets the ExtensionsToolbarContainer.
  virtual ExtensionsToolbarContainer* GetExtensionsToolbarContainer() = 0;

  // Gets the PinnedToolbarActionsContainer.
  virtual PinnedToolbarActionsContainer* GetPinnedToolbarActionsContainer() = 0;

  // Get the default size for toolbar buttons.
  virtual gfx::Size GetToolbarButtonSize() const = 0;

  // Gets the default view to use as an anchor for extension dialogs if the
  // ToolbarActionView is not visible or available.
  virtual views::View* GetDefaultExtensionDialogAnchorView() = 0;

  // Gets the specified page action icon. This function should only be used
  // if you need functionality for the legacy page action icon view. This
  // method will be removed after the migration is complete.
  virtual PageActionIconView* GetPageActionIconView(
      PageActionIconType type) = 0;

  // Gets the specified page action icon. This function can be used to retrieve
  // either the legacy page action icon view or the migrated page action view.
  virtual IconLabelBubbleView* GetPageActionView(
      actions::ActionId action_id) = 0;

  // Gets the app menu button.
  virtual AppMenuButton* GetAppMenuButton() = 0;

  // Returns a bounding box for the find bar in widget coordinates given the
  // bottom of the contents container.
  virtual gfx::Rect GetFindBarBoundingBox(int contents_bottom) = 0;

  // Gives the toolbar focus.
  virtual void FocusToolbar() = 0;

  // Returns the toolbar as an AccessiblePaneView.
  virtual views::AccessiblePaneView* GetAsAccessiblePaneView() = 0;

  // Returns the appropriate anchor view for the action id.
  //
  // Prefer `GetBubbleAnchor()` for new call sites.
  // `GetBubbleAnchor()` returns a `views::BubbleAnchor` which may contain
  // either a `views::View*` or a `ui::TrackedElement*` (or `nullptr`). Use
  // it when you need to support anchoring to WebUI DOM elements via
  // `TrackedElement` while remaining compatible with existing View anchors.
  // TODO(crbug.com/461070677): Replace GetAnchorView() with GetBubbleAnchor().
  virtual views::View* GetAnchorView(
      std::optional<actions::ActionId> action_id) = 0;

  // Returns the appropriate BubbleAnchor for the action id.
  virtual views::BubbleAnchor GetBubbleAnchor(
      std::optional<actions::ActionId> action_id) = 0;

  // See comment in browser_window.h for more info.
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // Returns the avatar button.
  virtual AvatarToolbarButton* GetAvatarToolbarButton() = 0;

  // Returns the back button.
  virtual ToolbarButton* GetBackButton() = 0;

  // Returns the reload button delegate, it can be either `ReloadButton` or
  // `ReloadButtonWebView` depending on the enabled features.
  virtual ReloadControl* GetReloadButton() = 0;

  // Returns the intent chip button, if present.
  virtual IntentChipButton* GetIntentChipButton() = 0;

  // Returns the download button.
  virtual ToolbarButton* GetDownloadButton() = 0;

  // TODO(calamity): Move other buttons and button actions into here.
 protected:
  virtual ~ToolbarButtonProvider() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
