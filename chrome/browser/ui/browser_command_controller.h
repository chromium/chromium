// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_
#define CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/command_updater_delegate.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class BrowserWindow;
class Profile;

namespace input {
struct NativeWebKeyboardEvent;
}

namespace chrome {

// This class needs to expose the internal command_updater_ in some way, hence
// it implements CommandUpdater as the public API for it (so it's not directly
// exposed).
class BrowserCommandController : public CommandUpdater,
                                 public TabStripModelObserver,
                                 public sessions::TabRestoreServiceObserver {
 public:
  explicit BrowserCommandController(Browser* browser);

  BrowserCommandController(const BrowserCommandController&) = delete;
  BrowserCommandController& operator=(const BrowserCommandController&) = delete;

  ~BrowserCommandController() override;

  // Returns true if |command_id| is a reserved command whose keyboard shortcuts
  // should not be sent to the renderer or |event| was triggered by a key that
  // we never want to send to the renderer.
  bool IsReservedCommandOrKey(int command_id,
                              const input::NativeWebKeyboardEvent& event);

  // Notifies the controller that state has changed in one of the following
  // areas and it should update command states.
  void TabStateChanged();
  void ZoomStateChanged();
  void ContentRestrictionsChanged();
  void FullscreenStateChanged();
#if BUILDFLAG(IS_CHROMEOS)
  // Called when the browser goes in or out of the special locked fullscreen
  // mode. In this mode the user is basically locked into the current browser
  // window and tab hence we disable most keyboard shortcuts and we also
  // prevent changing the state of enabled shortcuts while in this mode (so the
  // other *Changed() functions will be a NO-OP in this state).
  void LockedFullscreenStateChanged();
#endif
  void PrintingStateChanged();
  void LoadingStateChanged(bool is_loading, bool force);
  void FindBarVisibilityChanged();
  void ExtensionStateChanged();
  void TabKeyboardFocusChangedTo(std::optional<int> index);
  void WebContentsFocusChanged();

  // Overriden from CommandUpdater:
  bool SupportsCommand(int id) const override;
  bool IsCommandEnabled(int id) const override;
  bool ExecuteCommand(
      int id,
      base::TimeTicks time_stamp = base::TimeTicks::Now()) override;
  bool ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition disposition,
      base::TimeTicks time_stamp = base::TimeTicks::Now()) override;
  void AddCommandObserver(int id, CommandObserver* observer) override;
  void RemoveCommandObserver(int id, CommandObserver* observer) override;
  void RemoveCommandObserver(CommandObserver* observer) override;
  bool UpdateCommandEnabled(int id, bool state) override;

  // Shared state updating: these functions are static and public to share with
  // outside code.

  // Update commands whose state depends on incognito mode availability and that
  // only depend on the profile.
  static void UpdateSharedCommandsForIncognitoAvailability(
      CommandUpdater* command_updater,
      Profile* profile);

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  friend class BrowserCommandControllerBrowserTestLockedFullscreen;
#endif

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override;

  // Overridden from TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  // Returns true if the regular Chrome UI (not the fullscreen one and
  // not the single-tab one) is shown. Used for updating window command states
  // only. Consider using SupportsWindowFeature if you need the mentioned
  // functionality anywhere else.
  bool IsShowingMainUI();

  // Returns true if the location bar is shown or is currently hidden, but can
  // be shown. Used for updating window command states only.
  bool IsShowingLocationBar();

  // Initialize state for all browser commands.
  void InitCommandState();

  // Update commands whose state depends on incognito mode availability.
  void UpdateCommandsForIncognitoAvailability();

  // Update commands whose state depends on the tab's state.
  void UpdateCommandsForTabState();

  // Update Zoom commands based on zoom state.
  void UpdateCommandsForZoomState();

  // Updates commands when the content's restrictions change.
  void UpdateCommandsForContentRestrictionState();

  // Updates commands for enabling developer tools.
  void UpdateCommandsForDevTools();

  // Updates commands for bookmark editing.
  void UpdateCommandsForBookmarkEditing();

  // Updates commands that affect the bookmark bar.
  void UpdateCommandsForBookmarkBar();

  // Updates commands that affect file selection dialogs in aggregate,
  // namely the save-page-as state and the open-file state.
  void UpdateCommandsForFileSelectionDialogs();

  // Update commands whose state depends on the type of fullscreen mode the
  // window is in.
  void UpdateCommandsForFullscreenMode();

  // Update commands whose state depends on whether they're available to hosted
  // app windows.
  void UpdateCommandsForHostedAppAvailability();

  // Update commands that are used in the Extensions menu in the app menu.
  void UpdateCommandsForExtensionsMenu();

#if BUILDFLAG(IS_CHROMEOS)
  // Update commands whose state depends on whether the window is in locked
  // fullscreen mode or not.
  void UpdateCommandsForLockedFullscreenMode();
#endif

  // Updates the printing command state.
  void UpdatePrintingState();

  // Updates the SHOW_SYNC_SETUP menu entry.
  void OnSigninAllowedPrefChange();

  // Updates the save-page-as command state.
  void UpdateSaveAsState();

  // Ask the Reload/Stop button to change its icon, and update the Stop command
  // state.  |is_loading| is true if the current WebContents is loading.
  // |force| is true if the button should change its icon immediately.
  void UpdateReloadStopState(bool is_loading, bool force);

  void UpdateTabRestoreCommandState();

  // Updates commands for find.
  void UpdateCommandsForFind();

  // Updates the command to close find or stop loading.
  void UpdateCloseFindOrStop();

  // Updates commands for Media Router.
  void UpdateCommandsForMediaRouter();

  // Updates commands for tab keyboard focus state. If |target_index| is
  // populated, it is the index of the tab with focus; if it is not populated,
  // no tab has keyboard focus.
  void UpdateCommandsForTabKeyboardFocus(std::optional<int> target_index);

  // Updates commands that depend on whether web contents is focused or not.
  void UpdateCommandsForWebContentsFocus();

  // Updates commands that depend on the state of the tab strip model.
  void UpdateCommandsForTabStripStateChanged();

  // Returns the relevant action for the current browser for a given
  // `action_id`.
  actions::ActionItem* FindAction(actions::ActionId action_id);

  // Updates the enabled status for both `command_id` and `action_id`, given
  // that it exists.
  void UpdateCommandAndActionEnabled(int command_id,
                                     actions::ActionId action_id,
                                     bool enabled);

  // Helper method to show the customize chrome sidepanel and optionally scroll
  // to a specific section.
  void ShowCustomizeChromeSidePanel(
      std::optional<CustomizeChromeSection> section = std::nullopt);

  inline BrowserWindow* window();
  inline Profile* profile();

  const raw_ptr<Browser> browser_;

  // The CommandUpdaterImpl that manages the browser window commands.
  CommandUpdaterImpl command_updater_;

  PrefChangeRegistrar profile_pref_registrar_;
  PrefChangeRegistrar local_pref_registrar_;

  // In locked fullscreen mode disallow enabling/disabling commands.
  bool is_locked_fullscreen_ = false;

  // If the Customize Chrome side panel is shown, determines which section to
  // display.
  CustomizeChromeSection customize_chrome_section_ =
      CustomizeChromeSection::kUnspecified;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMAND_CONTROLLER_H_
