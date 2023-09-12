// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands_mac.h"

#include <unistd.h>

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen_util_mac.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chrome {

void ToggleAlwaysShowToolbarInFullscreen(Browser* browser) {
  DCHECK(browser);

  // If this browser belongs to an app, toggle the value for that app.
  web_app::AppBrowserController* app_controller = browser->app_controller();
  if (app_controller) {
    app_controller->ToggleAlwaysShowToolbarInFullscreen();
    return;
  }

  // Otherwise toggle the value of the preference.
  PrefService* prefs = browser->profile()->GetPrefs();
  bool show_toolbar = prefs->GetBoolean(prefs::kShowFullscreenToolbar);
  prefs->SetBoolean(prefs::kShowFullscreenToolbar, !show_toolbar);
}

void SetAlwaysShowToolbarInFullscreenForTesting(Browser* browser,  // IN-TEST
                                                bool always_show) {
  if (always_show == fullscreen_utils::IsAlwaysShowToolbarEnabled(browser)) {
    return;
  }
  ToggleAlwaysShowToolbarInFullscreen(browser);
}

void ToggleJavaScriptFromAppleEventsAllowed(Browser* browser) {
  CGEventRef cg_event = NSApp.currentEvent.CGEvent;
  if (!cg_event)
    return;

  // If the event is from another process, do not allow it to toggle this
  // secure setting.
  int sender_pid =
      CGEventGetIntegerValueField(cg_event, kCGEventSourceUnixProcessID);
  if (sender_pid != 0 && sender_pid != getpid()) {
    DLOG(ERROR)
        << "Dropping JS AppleScript toggle, event not from browser, from "
        << sender_pid;
    return;
  }

  // Only allow events generated in the HID system to toggle this setting.
  int event_source =
      CGEventGetIntegerValueField(cg_event, kCGEventSourceStateID);
  if (event_source != kCGEventSourceStateHIDSystemState) {
    DLOG(ERROR) << "Dropping JS AppleScript toggle, event source state not "
                   "from HID, from "
                << event_source;
    return;
  }

  PrefService* prefs = browser->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowJavascriptAppleEvents,
                    !prefs->GetBoolean(prefs::kAllowJavascriptAppleEvents));
}

}  // namespace chrome
