// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_

#include <stddef.h>

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class Browser;
class ChipController;
class CommandUpdater;
class LocationBarModel;
class LocationBarTesting;
class OmniboxController;
class OmniboxView;

namespace bubble_anchor_util {
struct AnchorConfiguration;
}

namespace content {
class WebContents;
}

namespace ui {
class MouseEvent;
class TrackedElement;
}

// The LocationBar class is a virtual interface, defining access to the
// window's location bar component.  This class exists so that cross-platform
// components like the browser command system can talk to the platform
// specific implementations of the location bar control.  It also allows the
// location bar to be mocked for testing.
class LocationBar {
 public:
  // Holds the details necessary to open the omnibox match via browser commands.
  struct NavigationParams {
    GURL destination_url;
    WindowOpenDisposition disposition{WindowOpenDisposition::CURRENT_TAB};
    ui::PageTransition transition{ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR};
    base::TimeTicks match_selection_timestamp;
    bool url_typed_without_scheme;
    bool url_typed_with_http_scheme;
    std::string_view extra_headers;
  };

  explicit LocationBar(CommandUpdater* command_updater)
      : command_updater_(command_updater) {}

  const NavigationParams& navigation_params() { return navigation_params_; }
  void set_navigation_params(NavigationParams navigation_params) {
    navigation_params_ = navigation_params;
  }

  // Focuses the location bar. User-initiated focuses (like pressing Ctrl+L)
  // should have |is_user_initiated| set to true. In those cases, we want to
  // take some extra steps, like selecting everything and maybe uneliding.
  //
  // Renderer-initiated focuses (like browser startup or NTP finished loading),
  // should have |is_user_initiated| set to false, so we can avoid disrupting
  // user actions and avoid requesting on-focus suggestions.
  //
  // If `clear_focus_if_failed` is true, the focus should be cleared entirely
  // if the location bar can't take it.
  virtual void FocusLocation(bool is_user_initiated,
                             bool clear_focus_if_failed) = 0;

  // Puts the user into keyword mode with their default search provider.
  // TODO(tommycli): See if there's a more descriptive name for this method.
  virtual void FocusSearch() = 0;

  // Adjust whether the location bar is focusable based on toolbar visibility.
  virtual void UpdateFocusBehavior(bool toolbar_visible) = 0;

  // Updates the state of the images showing the content settings status.
  virtual void UpdateContentSettingsIcons() = 0;

  // Saves the state of the location bar to the specified WebContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(content::WebContents* contents) = 0;

  // Reverts the location bar.  The bar's permanent text will be shown.
  virtual void Revert() = 0;

  virtual OmniboxView* GetOmniboxView() = 0;

  // Returns the OmniboxController owned by this LocationBar.
  virtual OmniboxController* GetOmniboxController() = 0;

  // Returns true if given mouse event should result in omnibox popup getting
  // closed.
  virtual bool ShouldCloseOmniboxPopup(ui::MouseEvent* event) = 0;

  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents() = 0;

  virtual LocationBarModel* GetLocationBarModel() = 0;

  // If chip is visible in this LocationBar, return a bubble anchor config that
  // can be used to anchor to the chip.
  virtual std::optional<bubble_anchor_util::AnchorConfiguration>
  GetChipAnchor() = 0;

  // Controls the chip in the LocationBar.
  virtual ChipController* GetChipController() = 0;

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged() = 0;

  // Called when the edit should update itself without restoring any tab state.
  virtual void UpdateWithoutTabRestore() = 0;

  CommandUpdater* command_updater() { return command_updater_; }
  const CommandUpdater* command_updater() const { return command_updater_; }

  // Warning: this may be null if the location bar is not visible.
  // Gets an anchor for the entire location bar.
  virtual ui::TrackedElement* GetAnchorOrNull() = 0;

  // Returns the Browser object this is for. This may be nullptr sometimes;
  // known cases include captive portals on ChromeOS and
  // PresentationReceiverWindowView.
  virtual Browser* GetBrowser() = 0;

  // Returns true if the location bar finished initializing --- it's linked to
  // the UI and has the subobjects all created.
  virtual bool IsInitialized() const = 0;

  // Returns true if the location bar is visible.
  virtual bool IsVisible() const = 0;

  // True if the location bar is drawn on screen; this is basically a recursive
  // equivalent of IsVisible() that also checks the parent UI elements.
  virtual bool IsDrawn() const = 0;

  // True if the window this location bar is in is in a full-screen mode.
  virtual bool IsFullscreen() const = 0;

  // Returns true if corresponding omnibox is editing text or empty.
  virtual bool IsEditingOrEmpty() const = 0;

  // Tells whatever UI system is used that it should recompute sizes of things.
  virtual void InvalidateLayout() = 0;

  // Returns the the location bar's bounds; see views::View::bounds().
  virtual gfx::Rect Bounds() const = 0;

  // Returns the minimum size of the location bar.
  virtual gfx::Size MinimumSize() const = 0;

  // Returns the preferred size of the location bar.
  virtual gfx::Size PreferredSize() const = 0;

  // Updates the controller, and, if `contents` is non-null, restores saved
  // state that the tab holds.
  virtual void Update(content::WebContents* contents) = 0;

  // Clears the location bar's state for `contents`.
  virtual void ResetTabState(content::WebContents* contents) = 0;

  // Returns true if the location bar's current security state does not match
  // the currently visible state.
  virtual bool HasSecurityStateChanged() = 0;

  // Returns a pointer to the testing interface.
  virtual LocationBarTesting* GetLocationBarForTesting() = 0;

 protected:
  virtual ~LocationBar() = default;

 private:
  NavigationParams navigation_params_;
  const raw_ptr<CommandUpdater, DanglingUntriaged> command_updater_;
};

class LocationBarTesting {
 public:
  // Invokes the content setting image at |index|, displaying the bubble.
  // Returns false if there is none.
  virtual bool TestContentSettingImagePressed(size_t index) = 0;

  // Returns if the content setting image at |index| is displaying a bubble.
  virtual bool IsContentSettingBubbleShowing(size_t index) = 0;

 protected:
  virtual ~LocationBarTesting() = default;
};

#endif  // CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
