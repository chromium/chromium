// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_LACROS_FIRST_RUN_SERVICE_H_
#define CHROME_BROWSER_UI_STARTUP_LACROS_FIRST_RUN_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#error This file should only be included on lacros.
#endif

class Profile;

// Task to run after the FRE is exited, with `proceed` indicating whether it
// should be aborted or resumed.
using ResumeTaskCallback = base::OnceCallback<void(bool proceed)>;

// Service handling the First Run Experience for the primary profile on Lacros.
// It is not available on the other profiles.
class LacrosFirstRunService : public KeyedService {
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

  explicit LacrosFirstRunService(Profile* profile);
  ~LacrosFirstRunService() override;

  // Returns whether first run experience (including sync promo) should be
  // opened on startup.
  bool ShouldOpenFirstRun() const;

  // Assuming that the first run experience needs to be opened on startup,
  // asynchronously attempts to complete it silently, in case collecting consent
  // is not needed. If `callback` is provided, it will run once the attempt is
  // completed. To see if it the attempt worked, call `ShouldOpenFirstRun()`.
  void TryMarkFirstRunAlreadyFinished(base::OnceClosure callback);

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
  void OpenFirstRunIfNeeded(EntryPoint entry_point,
                            ResumeTaskCallback callback);

 private:
  void OpenFirstRunInternal(EntryPoint entry_point,
                            ResumeTaskCallback callback);
  void TryEnableSyncSilentlyWithToken(const CoreAccountId& account_id,
                                      base::OnceClosure callback);

  // Owns of this instance via the KeyedService mechanism.
  const raw_ptr<Profile> profile_;

  std::unique_ptr<signin::IdentityManager::Observer> token_load_observer_;

  base::WeakPtrFactory<LacrosFirstRunService> weak_ptr_factory_{this};
};

class LacrosFirstRunServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  LacrosFirstRunServiceFactory(const LacrosFirstRunServiceFactory&) = delete;
  LacrosFirstRunServiceFactory& operator=(const LacrosFirstRunServiceFactory&) =
      delete;

  static LacrosFirstRunService* GetForBrowserContext(
      content::BrowserContext* context);

  static LacrosFirstRunServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<LacrosFirstRunServiceFactory>;

  LacrosFirstRunServiceFactory();
  ~LacrosFirstRunServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

namespace testing {

// Overrides the outcome of a check made during
// `LacrosFirstRunService::TryEnableSyncSilentlyWithToken()` to indicate that
// Sync is required for the primary profile, without having to mock policies or
// device settings.
class ScopedSyncRequiredInFirstRun {
 public:
  explicit ScopedSyncRequiredInFirstRun(bool required);
  ~ScopedSyncRequiredInFirstRun();

 private:
  absl::optional<bool> overriden_value_;
};

}  // namespace testing

// Helper to call `LacrosFirstRunService::ShouldOpenFirstRun()` without having
// to first obtain the service instance.
bool ShouldOpenPrimaryProfileFirstRun(Profile* profile);

#endif  // CHROME_BROWSER_UI_STARTUP_LACROS_FIRST_RUN_SERVICE_H_
