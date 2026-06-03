// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_

#include <optional>

#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class AppMenuControl;
class AvatarToolbarButtonInterface;
class BrowserWindowInterface;
class PinnedToolbarActions;
class ExtensionsToolbarDesktop;
class IntentChipButton;
class PageActionIconView;
class ReloadButton;
class ReloadControl;
class ToolbarButton;
class WebUIToolbarWebView;

namespace page_actions {
class PageActionViewInterface;
}  // namespace page_actions

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class AccessiblePaneView;
}  // namespace views

// An interface implemented by a view contains and provides access to toolbar
// buttons in a BrowserView.
class ToolbarButtonProvider {
 public:
  DECLARE_USER_DATA(ToolbarButtonProvider);

  static ToolbarButtonProvider* From(BrowserWindowInterface* browser);

  // Gets the ExtensionsToolbarDesktop.
  virtual ExtensionsToolbarDesktop* GetExtensionsToolbarDesktop() = 0;

  // Gets the PinnedToolbarActions.
  virtual PinnedToolbarActions* GetPinnedToolbarActions() = 0;

  // Get the default size for toolbar buttons.
  virtual gfx::Size GetToolbarButtonSize() const = 0;

  // Gets the default anchor for extension dialogs if the
  // ToolbarActionView is not visible or available.
  virtual views::BubbleAnchor GetDefaultExtensionDialogAnchor() = 0;

  // Gets the specified page action icon. This function should only be used
  // if you need functionality for the legacy page action icon view. This
  // method will be removed after the migration is complete.
  virtual PageActionIconView* GetPageActionIconView(
      PageActionIconType type) = 0;

  // Gets an interface representing the specified page action icon. This
  // function can be used to retrieve either the legacy page action icon view,
  // the migrated page action view, or the even newer WebUI page action icon.
  //
  // Consider if calling the PageActionController would make more sense than
  // directly accessing and manipulating view state.
  virtual page_actions::PageActionViewInterface* GetPageActionViewInterface(
      actions::ActionId action_id) = 0;

  // Gets the app menu control.
  virtual AppMenuControl* GetAppMenuControl() = 0;

  // Returns a bounding box for the find bar in widget coordinates given the
  // bottom of the contents container.
  virtual gfx::Rect GetFindBarBoundingBox(int contents_bottom) = 0;

  // Gives the toolbar focus.
  virtual void FocusToolbar() = 0;

  // Returns the toolbar as an AccessiblePaneView.
  virtual views::AccessiblePaneView* GetAsAccessiblePaneView() = 0;

  // Returns the appropriate BubbleAnchor for the action id.
  virtual views::BubbleAnchor GetBubbleAnchor(
      std::optional<actions::ActionId> action_id) = 0;

  // Returns the appropriate BubbleAnchor for the page action id.
  virtual views::BubbleAnchor GetPageActionBubbleAnchor(
      actions::ActionId action_id) = 0;

  // See comment in browser_window.h for more info.
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // Returns the avatar button interface.
  virtual AvatarToolbarButtonInterface* GetAvatarToolbarButtonInterface() = 0;

  // Returns the back button.
  virtual ToolbarButton* GetBackButton() = 0;

  // Returns the reload button delegate, it can be either `ReloadButton` or
  // `WebUIToolbarWebView` depending on the enabled features.
  virtual ReloadControl* GetReloadButton() = 0;

  // Returns the intent chip button, if present.
  virtual IntentChipButton* GetIntentChipButton() = 0;

  // Returns the download button.
  virtual ToolbarButton* GetDownloadButton() = 0;

  // Returns the WebUIToolbarWebView (if any) for testing.
  virtual WebUIToolbarWebView* GetWebUIToolbarViewForTesting() = 0;

  // TODO(calamity): Move other buttons and button actions into here.
 protected:
  virtual ~ToolbarButtonProvider() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
