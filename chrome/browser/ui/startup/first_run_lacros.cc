// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/first_run_lacros.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {

// Processes the outcome from the FRE and resumes the user's interrupted task.
// `original_intent_callback` should be run to allow the caller to resume what
// they were trying to do before they stopped to show the FRE. If the FRE's
// `status` is not `ProfilePicker::FirstRunExitStatus::kCompleted`, that
// `original_intent_callback` will be called with `proceed` set to false,
// otherwise it will be called with true. `post_first_run_callback` will be
// executed for completed flows, to perform tasks that the FRE requires after
// the interrupted task is resumed.
void OnFirstRunHasExited(ResumeTaskCallback original_intent_callback,
                         ProfilePicker::FirstRunExitStatus status,
                         base::OnceClosure post_first_run_callback) {
  if (status != ProfilePicker::FirstRunExitStatus::kQuitEarly) {
    // The user got to the last step, we can mark the FRE as finished, whether
    // we eventually proceed with the original intent or not.
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(lacros_prefs::kPrimaryProfileFirstRunFinished,
                            true);
  }

  bool proceed = status == ProfilePicker::FirstRunExitStatus::kCompleted;
  std::move(original_intent_callback).Run(proceed);

  if (proceed && post_first_run_callback)
    std::move(post_first_run_callback).Run();
}

void OpenPrimaryProfileFirstRunIfNeededWithProfile(
    ResumeTaskCallback& callback,
    Profile* profile,
    Profile::CreateStatus create_status) {
  if (create_status != Profile::CreateStatus::CREATE_STATUS_INITIALIZED)
    return;

  DCHECK(callback);
  DCHECK(profile->IsMainProfile());

  // The FRE is used to get the sync consent so there is no point in showing it
  // when the consent is already given. This applies for example to migrated
  // profiles.
  if (IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSync)) {
    auto exit_status = ProfilePicker::FirstRunExitStatus::kCompleted;
    if (ProfilePicker::IsLacrosFirstRunOpen()) {
      // The FRE artificially marks Sync as being consented while it's open, as
      // it needs to check the sync server for policies, the FRE is not actually
      // completed. We also can't proceed while the FRE is up, so we just quit.
      // TODO(https://crbug.com/1300109): The FRE window might be in the
      // background, or closing... Can these cases be handled better?
      exit_status = ProfilePicker::FirstRunExitStatus::kQuitEarly;
    }
    OnFirstRunHasExited(std::move(callback), exit_status, base::OnceClosure());
    return;
  }

  ProfilePicker::Show(ProfilePicker::Params::ForLacrosPrimaryProfileFirstRun(
      base::BindOnce(&OnFirstRunHasExited, std::move(callback))));
}

}  // namespace

bool ShouldOpenPrimaryProfileFirstRun() {
  if (!base::FeatureList::IsEnabled(switches::kLacrosNonSyncingProfiles)) {
    // Sync is already always forced, no point showing the FRE to ask the user
    // to sync.
    return false;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoFirstRun))
    return false;

  const PrefService* const pref_service = g_browser_process->local_state();
  return !pref_service->GetBoolean(
      lacros_prefs::kPrimaryProfileFirstRunFinished);
}

void OpenPrimaryProfileFirstRunIfNeeded(ResumeTaskCallback callback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  profile_manager->CreateProfileAsync(
      profile_manager->GetPrimaryUserProfilePath(),
      base::BindRepeating(&OpenPrimaryProfileFirstRunIfNeededWithProfile,
                          base::OwnedRef(std::move(callback))));
}
