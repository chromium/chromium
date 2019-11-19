// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_STATE_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_STATE_H_

#include <memory>
#include <string>

#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/ui_base_types.h"

class Browser;

namespace base {
class CommandLine;
class DictionaryValue;
}  // namespace base

namespace gfx {
class Rect;
}

class PrefService;

namespace chrome {

std::string GetWindowName(const Browser* browser);
// A "window placement dictionary" holds information about the size and location
// of the window that is stored in the given PrefService. If the window_name
// isn't the name of a registered preference it is assumed to be the name of an
// app and the AppWindowPlacement key is used to find the app's dictionary.
std::unique_ptr<DictionaryPrefUpdate> GetWindowPlacementDictionaryReadWrite(
    const std::string& window_name,
    PrefService* prefs);
// Returns NULL if the window corresponds to an app that doesn't have placement
// information stored in the preferences system.
const base::DictionaryValue* GetWindowPlacementDictionaryReadOnly(
    const std::string& window_name,
    PrefService* prefs);

bool ShouldSaveWindowPlacement(const Browser* browser);

// Returns true if the saved bounds for this window should be treated as the
// bounds of the content area, not the whole window.
bool SavedBoundsAreContentBounds(const Browser* browser);

void SaveWindowPlacement(const Browser* browser,
                         const gfx::Rect& bounds,
                         ui::WindowShowState show_state);

void SaveWindowWorkspace(const Browser* browser, const std::string& workspace);

// Return the |bounds| for the browser window to be used upon creation.
// The |show_state| variable will receive the desired initial show state for
// the window.
void GetSavedWindowBoundsAndShowState(const Browser* browser,
                                      gfx::Rect* bounds,
                                      ui::WindowShowState* show_state);

namespace internal {

// Updates window bounds and show state from the provided command-line. Part of
// implementation of GetSavedWindowBoundsAndShowState, but exposed for testing.
void UpdateWindowBoundsAndShowStateFromCommandLine(
    const base::CommandLine& command_line,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state);

}  // namespace internal

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_STATE_H_
