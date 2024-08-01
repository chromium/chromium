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
#include "url/gurl.h"

class CommandUpdater;
class LocationBarModel;
class LocationBarTesting;
class OmniboxView;

namespace content {
class WebContents;
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
  virtual void FocusLocation(bool is_user_initiated) = 0;

  // Puts the user into keyword mode with their default search provider.
  // TODO(tommycli): See if there's a more descriptive name for this method.
  virtual void FocusSearch() = 0;

  // Updates the state of the images showing the content settings status.
  virtual void UpdateContentSettingsIcons() = 0;

  // Saves the state of the location bar to the specified WebContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(content::WebContents* contents) = 0;

  // Reverts the location bar.  The bar's permanent text will be shown.
  virtual void Revert() = 0;

  virtual OmniboxView* GetOmniboxView() = 0;

  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents() = 0;

  virtual LocationBarModel* GetLocationBarModel() = 0;

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged() = 0;

  // Called when the omnibox popup is shown or hidden.
  virtual void OnPopupVisibilityChanged() = 0;

  // Called when the edit should update itself without restoring any tab state.
  virtual void UpdateWithoutTabRestore() = 0;

  CommandUpdater* command_updater() { return command_updater_; }
  const CommandUpdater* command_updater() const { return command_updater_; }

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
