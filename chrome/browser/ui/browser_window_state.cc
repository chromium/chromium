// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window_state.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace chrome {
namespace {

// Parse two comma-separated integers from str. Return true on success.
bool ParseCommaSeparatedIntegers(const std::string& str,
                                 int* ret_num1,
                                 int* ret_num2) {
  size_t num1_size = str.find_first_of(',');
  if (num1_size == std::string::npos)
    return false;

  size_t num2_pos = num1_size + 1;
  size_t num2_size = str.size() - num2_pos;
  int num1 = 0;
  int num2 = 0;
  if (!base::StringToInt(str.substr(0, num1_size), &num1) ||
      !base::StringToInt(str.substr(num2_pos, num2_size), &num2))
    return false;

  *ret_num1 = num1;
  *ret_num2 = num2;
  return true;
}

class WindowPlacementPrefUpdate : public DictionaryPrefUpdate {
 public:
  WindowPlacementPrefUpdate(PrefService* service,
                            const std::string& window_name)
      : DictionaryPrefUpdate(service, prefs::kAppWindowPlacement),
        window_name_(window_name) {}

  ~WindowPlacementPrefUpdate() override {}

  base::DictionaryValue* Get() override {
    base::DictionaryValue* all_apps_dict = DictionaryPrefUpdate::Get();
    base::DictionaryValue* this_app_dict_weak = NULL;
    if (!all_apps_dict->GetDictionary(window_name_, &this_app_dict_weak)) {
      auto this_app_dict = std::make_unique<base::DictionaryValue>();
      this_app_dict_weak = this_app_dict.get();
      all_apps_dict->Set(window_name_, std::move(this_app_dict));
    }
    return this_app_dict_weak;
  }

 private:
  const std::string window_name_;

  DISALLOW_COPY_AND_ASSIGN(WindowPlacementPrefUpdate);
};

}  // namespace

std::string GetWindowName(const Browser* browser) {
  if (browser->app_name().empty()) {
    return browser->is_type_normal() ? prefs::kBrowserWindowPlacement
                                     : prefs::kBrowserWindowPlacementPopup;
  }
  return browser->app_name();
}

std::unique_ptr<DictionaryPrefUpdate> GetWindowPlacementDictionaryReadWrite(
    const std::string& window_name,
    PrefService* prefs) {
  DCHECK(!window_name.empty());
  // A normal DictionaryPrefUpdate will suffice for non-app windows.
  if (prefs->FindPreference(window_name)) {
    return std::make_unique<DictionaryPrefUpdate>(prefs, window_name);
  }
  return std::unique_ptr<DictionaryPrefUpdate>(
      new WindowPlacementPrefUpdate(prefs, window_name));
}

const base::DictionaryValue* GetWindowPlacementDictionaryReadOnly(
    const std::string& window_name,
    PrefService* prefs) {
  DCHECK(!window_name.empty());
  if (prefs->FindPreference(window_name))
    return prefs->GetDictionary(window_name);

  const base::DictionaryValue* app_windows =
      prefs->GetDictionary(prefs::kAppWindowPlacement);
  if (!app_windows)
    return NULL;
  const base::DictionaryValue* to_return = NULL;
  app_windows->GetDictionary(window_name, &to_return);
  return to_return;
}

bool ShouldSaveWindowPlacement(const Browser* browser) {
  // Never track app popup windows that do not have a trusted source (i.e.
  // popup windows spawned by an app).  See similar code in
  //   SessionService::ShouldTrackBrowser().
  return !browser->deprecated_is_app() || browser->is_trusted_source();
}

bool SavedBoundsAreContentBounds(const Browser* browser) {
  // Applications other than web apps (such as devtools) save their window size.
  // Web apps, on the other hand, have the same behavior as popups, and save
  // their content bounds.
  bool is_app_with_window_bounds =
      browser->deprecated_is_app() &&
      !web_app::AppBrowserController::IsForWebAppBrowser(browser);
  return !browser->is_type_normal() && !is_app_with_window_bounds &&
         !browser->is_trusted_source();
}

void SaveWindowPlacement(const Browser* browser,
                         const gfx::Rect& bounds,
                         ui::WindowShowState show_state) {
  // Save to the session storage service, used when reloading a past session.
  // Note that we don't want to be the ones who cause lazy initialization of
  // the session service. This function gets called during initial window
  // showing, and we don't want to bring in the session service this early.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->SetWindowBounds(browser->session_id(), bounds, show_state);
}

void SaveWindowWorkspace(const Browser* browser, const std::string& workspace) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->SetWindowWorkspace(browser->session_id(), workspace);
}

void GetSavedWindowBoundsAndShowState(const Browser* browser,
                                      gfx::Rect* bounds,
                                      ui::WindowShowState* show_state) {
  DCHECK(browser);
  DCHECK(bounds);
  DCHECK(show_state);
  *bounds = browser->override_bounds();
  WindowSizer::GetBrowserWindowBoundsAndShowState(browser->app_name(), *bounds,
                                                  browser, bounds, show_state);

  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();

  internal::UpdateWindowBoundsAndShowStateFromCommandLine(parsed_command_line,
                                                          bounds, show_state);
}

namespace internal {

void UpdateWindowBoundsAndShowStateFromCommandLine(
    const base::CommandLine& command_line,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) {
  // Allow command-line flags to override the window size and position. If
  // either of these is specified then set the show state to NORMAL so that
  // they are immediately respected.
  if (command_line.HasSwitch(switches::kWindowSize)) {
    std::string str = command_line.GetSwitchValueASCII(switches::kWindowSize);
    int width, height;
    if (ParseCommaSeparatedIntegers(str, &width, &height)) {
      bounds->set_size(gfx::Size(width, height));
      *show_state = ui::SHOW_STATE_NORMAL;
    }
  }
  if (command_line.HasSwitch(switches::kWindowPosition)) {
    std::string str =
        command_line.GetSwitchValueASCII(switches::kWindowPosition);
    int x, y;
    if (ParseCommaSeparatedIntegers(str, &x, &y)) {
      bounds->set_origin(gfx::Point(x, y));
      *show_state = ui::SHOW_STATE_NORMAL;
    }
  }
}

}  // namespace internal

}  // namespace chrome
