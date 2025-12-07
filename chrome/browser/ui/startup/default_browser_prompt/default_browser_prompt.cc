// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"

#include <limits>
#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace {

// Do not show the prompt if "suppress_default_browser_prompt_for_version" in
// the initial preferences is set to the current version.
bool ShouldShowDefaultBrowserPromptForCurrentVersion() {
  const std::string disable_version_string =
      g_browser_process->local_state()->GetString(
          prefs::kBrowserSuppressDefaultBrowserPrompt);
  const base::Version disable_version(disable_version_string);
  DCHECK(disable_version_string.empty() || disable_version.IsValid());
  return !(disable_version.IsValid() &&
           disable_version == version_info::GetVersion());
}

void OnCheckIsDefaultBrowserFinished(
    Profile* profile,
    base::OnceCallback<void(bool)> done_callback,
    shell_integration::DefaultWebClientState state) {
  bool did_show_prompt = false;
  if (state == shell_integration::IS_DEFAULT) {
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    chrome::startup::default_prompt::ResetPromptPrefs(profile);
  } else if (state == shell_integration::NOT_DEFAULT &&
             shell_integration::CanSetAsDefaultBrowser() &&
             ShouldShowDefaultBrowserPromptForCurrentVersion()) {
    // Only show the prompt if some other program is the user's default browser.
    // In particular, don't show it if another install mode is default (e.g.,
    // don't prompt for Chrome Beta if stable Chrome is the default).
    did_show_prompt =
        DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  }
  std::move(done_callback).Run(did_show_prompt);
}

}  // namespace

void RegisterDefaultBrowserPromptPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kBrowserSuppressDefaultBrowserPrompt,
                               std::string());
  registry->RegisterStringPref(prefs::kDefaultBrowserPromptRefreshStudyGroup,
                               std::string());
}

// Migrates the last declined time from the old int pref (profile) to the new
// Time pref (local). Does not clear the old pref as it is still needed to
// preserve the original behavior for the duration of the experiment.
// TODO(326079444): After experiment is over, change this function to also clear
// the old pref.
void MigrateDefaultBrowserLastDeclinedPref(PrefService* profile_prefs) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    CHECK_IS_TEST();
    return;
  }

  const PrefService::Preference* old_last_declined_time_pref =
      profile_prefs->FindPreference(prefs::kDefaultBrowserLastDeclined);
  const PrefService::Preference* last_declined_time_pref =
      local_state->FindPreference(prefs::kDefaultBrowserLastDeclinedTime);

  if (old_last_declined_time_pref->IsDefaultValue()) {
    return;
  }

  base::Time old_last_declined_time = base::Time::FromInternalValue(
      profile_prefs->GetInt64(prefs::kDefaultBrowserLastDeclined));
  base::Time last_declined_time =
      local_state->GetTime(prefs::kDefaultBrowserLastDeclinedTime);

  // Migrate if the local pref has never been set before, or if the local pref's
  // value was migrated from a different profile and the current profile's pref
  // has a value that is more recent. It is not possible to overwrite a user-set
  // value for the local pref as both the new pref and the old pref are kept in
  // sync from the moment the new pref is introduced.
  if (last_declined_time_pref->IsDefaultValue() ||
      old_last_declined_time > last_declined_time) {
    local_state->SetTime(prefs::kDefaultBrowserLastDeclinedTime,
                         old_last_declined_time);
    if (local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount) == 0) {
      local_state->SetInteger(prefs::kDefaultBrowserDeclinedCount, 1);
    }
  }
}

void ShowDefaultBrowserPrompt(Profile* profile,
                              base::OnceCallback<void(bool)> done_callback) {
  // Do not check if Chrome is the default browser if there is a policy in
  // control of this setting.
  if (g_browser_process->local_state()->IsManagedPreference(
          prefs::kDefaultBrowserSettingEnabled)) {
    // Handling of the browser.default_browser_setting_enabled policy setting is
    // taken care of in BrowserProcessImpl.
    return;
  }

  scoped_refptr<shell_integration::DefaultBrowserWorker>(
      new shell_integration::DefaultBrowserWorker())
      ->StartCheckIsDefault(base::BindOnce(&OnCheckIsDefaultBrowserFinished,
                                           profile, std::move(done_callback)));
}

