// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_LANGUAGES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_LANGUAGES_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
class Profile;
#endif

namespace settings {

// Chrome "Languages" settings page UI handler.
class LanguagesHandler : public SettingsPageUIHandler {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  explicit LanguagesHandler(Profile* profile);
#else
  LanguagesHandler();
#endif

  LanguagesHandler(const LanguagesHandler&) = delete;
  LanguagesHandler& operator=(const LanguagesHandler&) = delete;

  ~LanguagesHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  // Returns the prospective UI language. May not match the actual UI language,
  // depending on the user's permissions and whether the language is substituted
  // for another locale.
  void HandleGetProspectiveUILanguage(const base::Value::List& args);

  // Changes the preferred UI language, provided the user is allowed to do so.
  // The actual UI language will not change until the next restart.
  void HandleSetProspectiveUILanguage(const base::Value::List& args);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<Profile> profile_;  // Weak pointer.
#endif
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_LANGUAGES_HANDLER_H_
