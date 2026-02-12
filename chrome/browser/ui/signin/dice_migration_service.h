// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Tracks whether the user has been migrated to explicitly signed-in state
// following the DICe migration flow.
extern const char kDiceMigrationMigrated[];

class DiceMigrationService : public KeyedService {
 public:
  explicit DiceMigrationService(Profile* profile);
  DiceMigrationService(const DiceMigrationService&) = delete;
  DiceMigrationService& operator=(const DiceMigrationService&) = delete;
  ~DiceMigrationService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  base::OneShotTimer& GetToastTriggerTimerForTesting();

 private:
  bool ForceMigrateUserIfEligible();

  raw_ptr<Profile> profile_ = nullptr;

  // Timer used to trigger the toast after a grace period.
  base::OneShotTimer toast_trigger_timer_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
