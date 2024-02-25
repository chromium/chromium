// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/languages_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#endif

namespace settings {

#if BUILDFLAG(IS_CHROMEOS_ASH)
LanguagesHandler::LanguagesHandler(Profile* profile) : profile_(profile) {}
#else
LanguagesHandler::LanguagesHandler() = default;
#endif

LanguagesHandler::~LanguagesHandler() = default;

void LanguagesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProspectiveUILanguage",
      base::BindRepeating(&LanguagesHandler::HandleGetProspectiveUILanguage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setProspectiveUILanguage",
      base::BindRepeating(&LanguagesHandler::HandleSetProspectiveUILanguage,
                          base::Unretained(this)));
}

void LanguagesHandler::HandleGetProspectiveUILanguage(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];

  AllowJavascript();

  std::string locale;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, an individual profile may have a preferred locale.
  locale = profile_->GetPrefs()->GetString(language::prefs::kApplicationLocale);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (locale.empty()) {
    locale = g_browser_process->local_state()->GetString(
        language::prefs::kApplicationLocale);
  }

  ResolveJavascriptCallback(callback_id, base::Value(locale));
}

void LanguagesHandler::HandleSetProspectiveUILanguage(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

#if BUILDFLAG(IS_WIN)
  PrefService* prefs = g_browser_process->local_state();
  const std::string& language_code = args[0].GetString();
  prefs->SetString(language::prefs::kApplicationLocale, language_code);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // Secondary users and public session users cannot change the locale.
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  const std::string& language_code = args[0].GetString();
  if (user &&
      user->GetAccountId() == user_manager->GetPrimaryUser()->GetAccountId() &&
      user->GetType() != user_manager::UserType::kPublicAccount) {
    profile_->ChangeAppLocale(language_code,
                              Profile::APP_LOCALE_CHANGED_VIA_SETTINGS);
  }
#endif
}

}  // namespace settings
