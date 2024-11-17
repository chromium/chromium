// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_

#include <optional>

#include "chrome/browser/ui/page_action/page_action_icon_type.h"

class AppMenuButton;
class AvatarToolbarButton;
class DownloadToolbarButtonView;
class ExtensionsToolbarContainer;
class IntentChipButton;
class PageActionIconView;
class ReloadButton;
class ToolbarButton;

namespace gfx {
class Rect;
class Size;
}

namespace views {
class AccessiblePaneView;
class View;
}

// An interface implemented by a view contains and provides access to toolbar
// buttons in a BrowserView.
class ToolbarButtonProvider {
 public:
  // Gets the ExtensionsToolbarContainer.
  virtual ExtensionsToolbarContainer* GetExtensionsToolbarContainer() = 0;

  // Get the default size for toolbar buttons.
  virtual gfx::Size GetToolbarButtonSize() const = 0;

  // Gets the default view to use as an anchor for extension dialogs if the
  // ToolbarActionView is not visible or available.
  virtual views::View* GetDefaultExtensionDialogAnchorView() = 0;

  // Gets the specified page action icon.
  virtual PageActionIconView* GetPageActionIconView(
      PageActionIconType type) = 0;

  // Gets the app menu button.
  virtual AppMenuButton* GetAppMenuButton() = 0;

  // Returns a bounding box for the find bar in widget coordinates given the
  // bottom of the contents container.
  virtual gfx::Rect GetFindBarBoundingBox(int contents_bottom) = 0;

  // Gives the toolbar focus.
  virtual void FocusToolbar() = 0;

  // Returns the toolbar as an AccessiblePaneView.
  virtual views::AccessiblePaneView* GetAsAccessiblePaneView() = 0;

  // Returns the appropriate anchor view for the page action icon.
  virtual views::View* GetAnchorView(
      std::optional<PageActionIconType> type) = 0;

  // See comment in browser_window.h for more info.
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // Returns the avatar button.
  virtual AvatarToolbarButton* GetAvatarToolbarButton() = 0;

  // Returns the back button.
  virtual ToolbarButton* GetBackButton() = 0;

  // Returns the reload button.
  virtual ReloadButton* GetReloadButton() = 0;

  // Returns the intent chip button, if present.
  virtual IntentChipButton* GetIntentChipButton() = 0;

  // Returns the download button.
  virtual DownloadToolbarButtonView* GetDownloadButton() = 0;

  // TODO(calamity): Move other buttons and button actions into here.
 protected:
  virtual ~ToolbarButtonProvider() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOOLBAR_BUTTON_PROVIDER_H_
