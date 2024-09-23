// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FIRST_RUN_SERVICE_H_
#define CHROME_BROWSER_UI_STARTUP_FIRST_RUN_SERVICE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_buildflags.h"

class PrefRegistrySimple;
class Profile;
class SilentSyncEnabler;
class ProfileNameResolver;

namespace version_info {
enum class Channel;
}

namespace signin {
class IdentityManager;
}

// Task to run after the FRE is exited, with `proceed` indicating whether it
// should be aborted or resumed.
using ResumeTaskCallback = base::OnceCallback<void(bool proceed)>;

// Service handling the First Run Experience for the primary profile on Lacros.
// It is not available on the other profiles.
class FirstRunService : public KeyedService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class EntryPoint {
    // Indicates misc, undifferentiated entry points to the FRE that we don't
    // particularly worry about. If we have a concern about a specific entry
    // point, we should register a dedicated value for it to track how often it
    // gets triggered.
    kOther = 0,

    kProcessStartup = 1,
    kWebAppLaunch = 2,
    kWebAppContextMenu = 3,

    kMaxValue = kWebAppContextMenu
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FinishedReason {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    kExperimentCounterfactual = 0,
#endif
    kFinishedFlow = 1,
    kProfileAlreadySetUp = 2,
    kSkippedByPolicies = 3,
    // This is currently only used when the feature
    // `kForceSigninFlowInProfilePicker` is enabled.
    kForceSignin = 4,

    kMaxValue = kForceSignin,
  };

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  FirstRunService(Profile& profile, signin::IdentityManager& identity_manager);
  ~FirstRunService() override;

  // Runs `::ShouldOpenFirstRun(Profile*)` with the profile associated with this
  // service instance.
  virtual bool ShouldOpenFirstRun() const;

  // This function takes the user through the browser FRE.
  // 1) First, it checks whether the FRE flow can be skipped in the first place.
  //    This is the case when sync consent is already given (true for existing
  //    users that migrated to lacros) or when enterprise policies forbid the
  //    FRE. If so, the call directly 'finishes' the flow (see below).
  // 2) Then, it opens the FRE UI (in the profile picker window) and
  //    asynchronously 'finishes' the flow (sets a flag in the local prefs) once
  //    the user chooses any action on the sync consent screen. If the user
  //    exits the FRE UI via the generic 'Close window' affordances, it is
  //    interpreted as an intent to exit the app and `callback` will be called
  //    with `proceed` set to false. If they exit it via the dedicated options
  //    in the flow, it will be considered 'completed' and `callback` will be
  //    run with `proceed` set to true. If the FRE flow is exited before the
  //    sync consent screen, the flow is considered 'aborted', and can be shown
  //    again at the next startup.
  // When this method is called again while FRE is in progress, the previous
  // callback is aborted (called with false), and is replaced by `callback`.
  virtual void OpenFirstRunIfNeeded(EntryPoint entry_point,
                                    ResumeTaskCallback callback);

  // Terminates the first run without re-opening a browser window.
  virtual void FinishFirstRunWithoutResumeTask();

 private:
  friend class FirstRunServiceFactory;
  FRIEND_TEST_ALL_PREFIXES(FirstRunServiceTest,
                           ShouldPopulateProfileNameFromPrimaryAccount);

  // Asynchronously attempts to complete the first run silently.
  // By the time `callback` is run (if non-null), either:
  // - the first run has been marked finished because it can't be run for this
  //   profile (e.g. policies) or because we want to enable Sync silently (on
  //   Lacros only)
  // - the first run is ready to be opened.
  // The finished state can be checked by calling `ShouldOpenFirstRun()`.
  void TryMarkFirstRunAlreadyFinished(base::OnceClosure callback);

  void OpenFirstRunInternal(EntryPoint entry_point);

  // Processes the outcome from the FRE and resumes the user's interrupted task.
  void OnFirstRunHasExited(ProfilePicker::FirstRunExitStatus status);

  // Marks the first run as finished and updates the profile entry based on
  // the info obtained during the first run.
  // Noting that the latter part is done by calling `FinishProfileSetUp()`,
  // which will be done asynchronously in most cases.
  void FinishFirstRun(FinishedReason reason);

  void FinishProfileSetUp(std::u16string profile_name);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void StartSilentSync(base::OnceClosure callback);
  void ClearSilentSyncEnabler();
#endif

  // Owns of this instance via the KeyedService mechanism.
  const raw_ref<Profile> profile_;

  // KeyedService(s) this service depends on:
  const raw_ref<signin::IdentityManager> identity_manager_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<SilentSyncEnabler> silent_sync_enabler_;
#endif

  std::unique_ptr<ProfileNameResolver> profile_name_resolver_;

  ResumeTaskCallback resume_task_callback_;

  base::WeakPtrFactory<FirstRunService> weak_ptr_factory_{this};
};

class FirstRunServiceFactory : public ProfileKeyedServiceFactory {
 public:
  FirstRunServiceFactory(const FirstRunServiceFactory&) = delete;
  FirstRunServiceFactory& operator=(const FirstRunServiceFactory&) = delete;

  static FirstRunService* GetForBrowserContext(
      content::BrowserContext* context);

  static FirstRunService* GetForBrowserContextIfExists(
      content::BrowserContext* context);

  static FirstRunServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<FirstRunServiceFactory>;
  friend class FirstRunServiceBrowserTestBase;

  FirstRunServiceFactory();
  ~FirstRunServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

// Returns whether the first run experience (including sync promo) might be
// opened for `profile`. It should be checked before
// `FirstRunService::OpenFirstRunIfNeeded()` is called.
//
// Even if this method returns `true`, the FRE can still be skipped if for
// example the feature is disabled, a policy suppresses it, etc.
bool ShouldOpenFirstRun(Profile* profile);

#endif  // CHROME_BROWSER_UI_STARTUP_FIRST_RUN_SERVICE_H_
