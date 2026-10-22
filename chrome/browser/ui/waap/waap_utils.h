// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_

#include <memory>

#include "base/time/time.h"
#include "url/gurl.h"

class Profile;

namespace content {
class SecurityPrincipal;
class WebContents;
}  // namespace content

namespace waap {

// Represents the source of a new browser window creation. Used to slice the
// InitialWebUI.NewWindow.* histograms. The first window that paints in the
// browser process is recorded under InitialWebUI.Startup.* instead, whatever
// its source.
// LINT.IfChange(NewWindowCreationSource)
enum class NewWindowCreationSource {
  kUnknown = 0,
  // kBrowserInitiated = 1,  // Deprecated; split into specific sources below.
  // Created by dragging a tab out of a tab strip into a new window via
  // `TabDragController`.
  kDragToNewWindow = 2,
  // Created by session restore (`SessionRestoreImpl`) for a window restored
  // from a saved session.
  kSessionRestore = 3,
  // Created via `chrome::OpenEmptyWindow()`, e.g. keyboard shortcut
  // (Ctrl/Cmd+N), app menu "New window" / "New Incognito window", macOS Dock
  // click when no window is visible, or features such as Glic or background
  // mode opening a window when the profile has none. The window usually starts
  // with a single NTP tab, but startup tabs may be added during browser
  // creation when `should_trigger_session_restore` is true.
  kNewEmptyWindow = 4,
  // Created by moving existing tab(s) into a new window via
  // `chrome::MoveTabsToNewWindow()`, e.g. from the tab context menu.
  kMoveTabsToNewWindow = 5,
  // Created by a browser launch in
  // `StartupBrowserCreatorImpl::OpenTabsInBrowser()` (i.e.
  // `BrowserWindowCreateParams::CreationSource::kStartupCreator`) when no
  // existing tabbed browser is reused, e.g. an OS/shell shortcut (taskbar,
  // desktop, Start menu) or `--new-window` handled by an already running
  // browser process, or opening a window for another profile or Guest mode.
  kLaunch = 6,
  // Created by `SessionRestoreImpl::FinishedTabCreation()` as a fallback when
  // the restored session has no tabbed browser windows. Unlike
  // `kSessionRestore`, this window restores no tabs; it only opens the startup
  // tabs or the NTP.
  kSessionRestoreFallback = 7,
  kMaxValue = kSessionRestoreFallback,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/histograms.xml:CreationSource)

// Result of initial surface synchronization for the WebUI toolbar.
// LINT.IfChange(InitialWebUISurfaceSyncResult)
enum class InitialWebUISurfaceSyncResult {
  kReadyWithinDeadline = 0,
  kDeadlineExceededPaintedLater = 1,
  kDeadlineExceededClosedBeforePaint = 2,
  kDeadlineExceededRenderProcessGone = 3,
  kClosedBeforeBrowserPresentation = 4,
  kRenderProcessGoneBeforeBrowserPresentation = 5,
  kMaxValue = kRenderProcessGoneBeforeBrowserPresentation,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:InitialWebUISurfaceSyncResult)

// Returns true if the given URL or SecurityPrincipal is for the initial WebUI.
// This is only relevant on non-Android platforms.
bool IsForInitialWebUI(const GURL& url);
bool IsForInitialWebUI(const content::SecurityPrincipal& principal);

class PrewarmHelper {
 public:
  // Configures the WebContents used for the initial WebUI (e.g. page load
  // metrics, background color, zoom gestures, and color provider source).
  static void ConfigureWebUIContents(content::WebContents* web_contents,
                                     Profile* profile);

  // Prewarms the WebUI toolbar WebContents for the given profile.
  // Creates the WebContents and configures it using ConfigureWebUIContents.
  // If `pre_navigate` is true, starts loading the toolbar URL; otherwise, only
  // initializes the renderer process.
  static std::unique_ptr<content::WebContents> PrewarmWebUIContents(
      Profile* profile,
      bool pre_navigate);
};

}  // namespace waap

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
