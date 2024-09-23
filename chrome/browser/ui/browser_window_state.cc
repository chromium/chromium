// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/browser_window_state.h"

#include <stddef.h>

#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace chrome {
namespace {

// Parse two comma-separated integers from str. Return true on success.
bool ParseCommaSeparatedIntegers(const std::string& str,
                                 int* ret_num1,
                                 int* ret_num2) {
  const size_t comma = str.find(',');
  if (comma == std::string::npos) {
    return false;
  }
  auto view = std::string_view(str);
  return base::StringToInt(view.substr(0, comma), ret_num1) &&
         base::StringToInt(view.substr(comma + 1), ret_num2);
}

}  // namespace

std::string GetWindowName(const Browser* browser) {
  switch (browser->type()) {
    case Browser::TYPE_NORMAL:
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Browser::TYPE_CUSTOM_TAB:
#endif
      return prefs::kBrowserWindowPlacement;
    case Browser::TYPE_POPUP:
    case Browser::TYPE_PICTURE_IN_PICTURE:
      return prefs::kBrowserWindowPlacementPopup;
    case Browser::TYPE_APP:
    case Browser::TYPE_DEVTOOLS:
      return browser->app_name();
    case Browser::TYPE_APP_POPUP:
      return browser->app_name() + "_popup";
  }
}

base::Value::Dict& GetWindowPlacementDictionaryReadWrite(
    const std::string& window_name,
    PrefService* prefs,
    std::unique_ptr<ScopedDictPrefUpdate>& scoped_update) {
  DCHECK(!window_name.empty());
  // Non-app window placements each use their own per-window-name dictionary
  // preference, so can make a ScopedDictPrefUpdate for the relevant preference,
  // and return its dictionary directly.
  if (prefs->FindPreference(window_name)) {
    scoped_update = std::make_unique<ScopedDictPrefUpdate>(prefs, window_name);
    return scoped_update->Get();
  }

  // The window placements for all apps are stored in a single dictionary
  // preference, with per-window-name nested dictionaries, so need to make
  // ScopedDictPrefUpdate and then find the relevant dictionary within it, based
  // on window name.
  scoped_update =
      std::make_unique<ScopedDictPrefUpdate>(prefs, prefs::kAppWindowPlacement);
  base::Value::Dict* this_app_dict =
      (*scoped_update)->FindDictByDottedPath(window_name);
  if (this_app_dict)
    return *this_app_dict;
  return (*scoped_update)
      ->SetByDottedPath(window_name, base::Value::Dict())
      ->GetDict();
}

const base::Value::Dict* GetWindowPlacementDictionaryReadOnly(
    const std::string& window_name,
    PrefService* prefs) {
  DCHECK(!window_name.empty());
  if (prefs->FindPreference(window_name))
    return &prefs->GetDict(window_name);

  const base::Value::Dict& app_windows =
      prefs->GetDict(prefs::kAppWindowPlacement);
  return app_windows.FindDict(window_name);
}

bool ShouldSaveWindowPlacement(const Browser* browser) {
  // Never track app windows that do not have a trusted source (i.e. windows
  // spawned by an app).  See similar code in
  // SessionServiceBase::ShouldTrackBrowser().
  return !(browser->is_type_app() || browser->is_type_app_popup()) ||
         browser->is_trusted_source();
}

bool SavedBoundsAreContentBounds(const Browser* browser) {
  // Applications other than web apps (such as devtools) save their window size.
  // Web apps, on the other hand, have the same behavior as popups, and save
  // their content bounds.
  return !browser->is_type_normal() && !browser->is_type_devtools() &&
         !browser->is_trusted_source();
}

void SaveWindowPlacement(const Browser* browser,
                         const gfx::Rect& bounds,
                         ui::mojom::WindowShowState show_state) {
  // Save to the session storage service, used when reloading a past session.
  // Note that we don't want to be the ones who cause lazy initialization of
  // the session service. This function gets called during initial window
  // showing, and we don't want to bring in the session service this early.
  SessionServiceBase* service = GetAppropriateSessionServiceIfExisting(browser);
  if (service)
    service->SetWindowBounds(browser->session_id(), bounds, show_state);
}

void SaveWindowWorkspace(const Browser* browser, const std::string& workspace) {
  SessionServiceBase* service = GetAppropriateSessionServiceIfExisting(browser);
  if (service)
    service->SetWindowWorkspace(browser->session_id(), workspace);
}

void SaveWindowVisibleOnAllWorkspaces(const Browser* browser,
                                      bool visible_on_all_workspaces) {
  SessionServiceBase* service = GetAppropriateSessionServiceIfExisting(browser);
  if (service) {
    service->SetWindowVisibleOnAllWorkspaces(browser->session_id(),
                                             visible_on_all_workspaces);
  }
}

void GetSavedWindowBoundsAndShowState(const Browser* browser,
                                      gfx::Rect* bounds,
                                      ui::mojom::WindowShowState* show_state) {
  DCHECK(browser);
  DCHECK(bounds);
  DCHECK(show_state);
  *bounds = browser->override_bounds();
  WindowSizer::GetBrowserWindowBoundsAndShowState(*bounds, browser, bounds,
                                                  show_state);

  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();

  internal::UpdateWindowBoundsAndShowStateFromCommandLine(parsed_command_line,
                                                          bounds, show_state);
}

namespace internal {

void UpdateWindowBoundsAndShowStateFromCommandLine(
    const base::CommandLine& command_line,
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) {
  // Allow command-line flags to override the window size and position. If
  // either of these is specified then set the show state to NORMAL so that
  // they are immediately respected.
  if (command_line.HasSwitch(switches::kWindowSize)) {
    std::string str = command_line.GetSwitchValueASCII(switches::kWindowSize);
    int width, height;
    if (ParseCommaSeparatedIntegers(str, &width, &height)) {
      bounds->set_size(gfx::Size(width, height));
      *show_state = ui::mojom::WindowShowState::kNormal;
    }
  }
  if (command_line.HasSwitch(switches::kWindowPosition)) {
    std::string str =
        command_line.GetSwitchValueASCII(switches::kWindowPosition);
    int x, y;
    if (ParseCommaSeparatedIntegers(str, &x, &y)) {
      bounds->set_origin(gfx::Point(x, y));
      *show_state = ui::mojom::WindowShowState::kNormal;
    }
  }
}

}  // namespace internal

}  // namespace chrome
