// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/size.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class BrowserWindow;
class ToolbarActionsBarDelegate;
class ToolbarActionsBarObserver;
class ToolbarActionViewController;

// A platform-independent version of the container for toolbar actions,
// including extension actions and component actions.
//
// This is a per-window instance, unlike the ToolbarActionsModel, which is
// per-profile. In most cases, ordering and visible count will be identical
// between the base model and the window; however, there are exceptions in the
// case of very small windows (which may be too narrow to display all the
// icons), or windows in which an action is "popped out", resulting in a
// re-ordering.
//
// This can come in two flavors, main and "overflow". The main bar is visible
// next to the omnibox, and the overflow bar is visible inside the chrome
// app menu. The main bar can have only a single row of icons with flexible
// width, whereas the overflow bar has multiple rows of icons with a fixed
// width (the width of the menu).
class ToolbarActionsBar : public ExtensionsContainer,
                          public ToolbarActionsModel::Observer,
                          public TabStripModelObserver {
 public:
  using ToolbarActions =
      std::vector<std::unique_ptr<ToolbarActionViewController>>;

  // A struct to contain the platform settings.
  struct PlatformSettings {
    PlatformSettings();

    // The spacing between each of the icons, between the start of the
    // container and the first item, and between the last item and end of
    // the container.
    int item_spacing;
    // The number of icons per row in the overflow menu.
    int icons_per_overflow_menu_row;
  };

  // The type of drag that occurred in a drag-and-drop operation.
  enum DragType {
    // The icon was dragged to the same container it started in.
    DRAG_TO_SAME,
    // The icon was dragged from the main container to the overflow.
    DRAG_TO_OVERFLOW,
    // The icon was dragged from the overflow container to the main.
    DRAG_TO_MAIN,
  };

  enum HighlightType {
    HIGHLIGHT_NONE,
    HIGHLIGHT_WARNING,
  };

  ToolbarActionsBar(ToolbarActionsBarDelegate* delegate,
                    Browser* browser,
                    ToolbarActionsBar* main_bar);
  ~ToolbarActionsBar() override;

  // Gets the ToolbarActionsBar from the given BrowserWindow. This method is
  // essentially deprecated. Use BrowserWindow::GetExtensionsContainer instead.
  static ToolbarActionsBar* FromBrowserWindow(BrowserWindow* window);

  // Registers profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the size of the area where the action icon resides.
  static gfx::Size GetIconAreaSize();

  // Returns the size of ToolbarActionView.
  gfx::Size GetViewSize() const;

  // Returns the default/full size for the toolbar; this does *not* reflect any
  // animations that may be running.
  gfx::Size GetFullSize() const;

  // Returns the [minimum|maximum] possible width for the toolbar.
  virtual int GetMinimumWidth() const;
  int GetMaximumWidth() const;

  // Returns the width for the given number of icons.
  int IconCountToWidth(size_t icons) const;

  // Returns the number of icons that can fit within the given width.
  size_t WidthToIconCount(int width) const;

  // Returns the number of icons that should be displayed if space allows. Can
  // be overridden by children to impose a smaller limit on the number of icons.
  virtual size_t GetIconCount() const;

  // Returns the starting index (inclusive) for displayable icons.
  size_t GetStartIndexInBounds() const;

  // Returns the ending index (exclusive) for displayable icons.
  size_t GetEndIndexInBounds() const;

  // Returns true if an overflow container is necessary to display any other
  // icons for this particular window. This is different than
  // ToolbarActionsModel::all_icons_visible() because the ToolbarActionsBar
  // is limited to a single window, whereas the model is the underlying model
  // of *all* windows, independent of size. As such, the model is identical
  // between a very wide window and a very narrow window, and the user's stored
  // preference may be to have all icons visible. But if the very narrow window
  // doesn't have the width to display all those actions, some will need to be
  // implicitly pushed to the overflow, even though the user's global preference
  // has not changed.
  bool NeedsOverflow() const;

  // Returns the frame (bounds) that the specified index should have, taking
  // into account if this is the main or overflow bar. If this is the overflow
  // bar and the index should not be displayed (i.e., it is shown on the main
  // bar), returns an empty rect.
  gfx::Rect GetFrameForIndex(size_t index) const;

  // Returns the actions in the proper order; this may differ from the
  // underlying order in the case of actions being popped out to show a popup.
  std::vector<ToolbarActionViewController*> GetActions() const;

  // Creates the toolbar actions.
  void CreateActions();

  // Deletes all toolbar actions.
  void DeleteActions();

  // Updates all the toolbar actions.
  void Update();

  // Sets the width for the overflow menu rows.
  void SetOverflowRowWidth(int width);

  // Notifies the ToolbarActionsBar that a user completed a resize gesture, and
  // the new width is |width|.
  void OnResizeComplete(int width);

  // Notifies the ToolbarActionsBar that the user has started dragging the
  // action at index |index_of_dragged_item|.
  void OnDragStarted(size_t index_of_dragged_item);

  // Notifies the ToolbarActionsBar that a drag-and-drop sequence ended. This
  // may not coincide with OnDragDrop(), since the view may be dropped somewhere
  // else.
  void OnDragEnded();

  // Notifies the ToolbarActionsBar that a user completed a drag and drop event,
  // and dragged the view from |dragged_index| to |dropped_index|.
  // |drag_type| indicates whether or not the icon was dragged between the
  // overflow and main containers.
  // The main container should handle all drag/drop notifications.
  void OnDragDrop(int dragged_index,
                  int dropped_index,
                  DragType drag_type);

  // The index of the action currently being dragged, or |base::nullopt| if
  // no drag is in progress. Should only be called on the main bar.
  const base::Optional<size_t> IndexOfDraggedItem() const;

  // Notifies the ToolbarActionsBar that the delegate finished animating.
  void OnAnimationEnded();

  // Called when the active bubble is closed.
  void OnBubbleClosed();

  // Add or remove an observer.
  void AddObserver(ToolbarActionsBarObserver* observer);
  void RemoveObserver(ToolbarActionsBarObserver* observer);

  // Returns the underlying toolbar actions, but does not order them. Primarily
  // for use in testing.
  const ToolbarActions& toolbar_actions_unordered() const {
    return toolbar_actions_;
  }
  bool enabled() const { return model_ != nullptr; }
  bool suppress_layout() const { return suppress_layout_; }
  bool suppress_animation() const {
    return suppress_animation_ || disable_animations_for_testing_;
  }
  bool is_highlighting() const { return model_ && model_->is_highlighting(); }
  ToolbarActionsModel::HighlightType highlight_type() const {
    return model_ ? model_->highlight_type()
                  : ToolbarActionsModel::HIGHLIGHT_NONE;
  }
  const PlatformSettings& platform_settings() const {
    return platform_settings_;
  }
  ToolbarActionViewController* popup_owner() { return popup_owner_; }
  bool in_overflow_mode() const { return main_bar_ != nullptr; }
  bool is_showing_bubble() const { return is_showing_bubble_; }

  bool is_drag_in_progress() const {
    return index_of_dragged_item_ != base::nullopt;
  }

  ToolbarActionsBarDelegate* delegate_for_test() { return delegate_; }

  // During testing we can disable animations by setting this flag to true,
  // so that the bar resizes instantly, instead of having to poll it while it
  // animates to open/closed status.
  static bool disable_animations_for_testing_;
  static void set_extension_bubble_appearance_wait_time_for_testing(
      int time_in_seconds);

  // ExtensionsContainer:
  ToolbarActionViewController* GetActionForId(
      const std::string& action_id) override;
  ToolbarActionViewController* GetPoppedOutAction() const override;
  bool IsActionVisibleOnToolbar(
      const ToolbarActionViewController* action) const override;
  extensions::ExtensionContextMenuModel::ButtonVisibility GetActionVisibility(
      const ToolbarActionViewController* action) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewController* popup_owner) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  void PopOutAction(ToolbarActionViewController* action,
                    bool is_sticky,
                    const base::Closure& closure) override;
  bool ShowToolbarActionPopupForAPICall(const std::string& id) override;
  void ShowToolbarActionBubble(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) override;
  void ShowToolbarActionBubbleAsync(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) override;

 private:
  // Returns the insets by which the icon area bounds (See GetIconAreaRect())
  // are insetted. This defines the amount of paddings around the icon area.
  virtual gfx::Insets GetIconAreaInsets() const;

  // Returns the number of icons that can fit within the given width.
  size_t WidthToIconCountUnclamped(int width) const;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionMoved(const ToolbarActionsModel::ActionId& action_id,
                            int index) override;
  void OnToolbarActionLoadFailed() override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarVisibleCountChanged() override;
  void OnToolbarHighlightModeChanged(bool is_highlighting) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Resizes the delegate (if necessary) to the preferred size using the given
  // |tween_type|.
  void ResizeDelegate(gfx::Tween::Type tween_type);

  // Returns the current web contents.
  content::WebContents* GetCurrentWebContents();

  // Reorders the toolbar actions to reflect the model and, optionally, to
  // "pop out" any overflowed actions that want to run (depending on the
  // value of |pop_out_actions_to_run|.
  void ReorderActions();

  // Shows an extension message bubble, if any should be shown.
  void MaybeShowExtensionBubble();

  // Returns the main bar, which is |main_bar_| if this is in overflow mode, and
  // |this| otherwise.
  ToolbarActionsBar* GetMainBar();

  // The delegate for this object (in a real build, this is the view).
  ToolbarActionsBarDelegate* delegate_;

  // The associated browser.
  Browser* const browser_;

  // The observed toolbar model.
  ToolbarActionsModel* model_;

  // The controller for the main toolbar actions bar. This will be null if this
  // is the main bar.
  ToolbarActionsBar* main_bar_;

  // Platform-specific settings for dimensions.
  PlatformSettings platform_settings_;

  // The toolbar actions.
  ToolbarActions toolbar_actions_;

  // The action that triggered the current popup (just a reference to an action
  // from toolbar_actions_).
  ToolbarActionViewController* popup_owner_;

  ScopedObserver<ToolbarActionsModel, ToolbarActionsModel::Observer>
      model_observer_;

  // True if we should suppress layout, such as when we are creating or
  // adjusting a lot of actions at once.
  bool suppress_layout_;

  // True if we should suppress animation; we do this when first creating the
  // toolbar, and also when switching tabs changes the state of the icons.
  bool suppress_animation_;

  // If this is true, actions that want to run (e.g., an extension's page
  // action) will pop out of overflow to draw more attention.
  // See also TabOrderHelper in the .cc file.
  static bool pop_out_actions_to_run_;

  // True if we should check to see if there is an extension bubble that should
  // be displayed, and, if there is, started the process for showing that
  // bubble. This is only ever true for the main bar.
  bool should_check_extension_bubble_;

  // The action, if any, which is currently "popped out" of the overflow in
  // order to show a popup.
  ToolbarActionViewController* popped_out_action_;

  // True if the popped out action is "sticky", meaning it will stay popped
  // out even if another menu is opened.
  bool is_popped_out_sticky_;

  // The task to alert the |popped_out_action_| that animation has finished, and
  // it is fully popped out.
  base::Closure popped_out_closure_;

  // The controller for the toolbar action bubble to show once animation
  // finishes, if any.
  std::unique_ptr<ToolbarActionsBarBubbleDelegate> pending_bubble_controller_;

  // True if a bubble is currently being shown.
  bool is_showing_bubble_;

  // The index of the action currently being dragged, or |base::nullopt| if
  // no drag is in progress.
  base::Optional<size_t> index_of_dragged_item_;

  base::ObserverList<ToolbarActionsBarObserver>::Unchecked observers_;

  base::WeakPtrFactory<ToolbarActionsBar> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBar);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_H_
