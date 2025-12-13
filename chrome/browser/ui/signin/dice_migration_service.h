// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class BrowserWindowInterface;
class Profile;
namespace signin {
class AccountManagedStatusFinder;
}  // namespace signin
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
namespace views {
class Widget;
}  // namespace views

// Tracks the number of times the DICe migration dialog has been shown.
extern const char kDiceMigrationDialogShownCount[];

// Tracks the last time the DICe migration dialog was shown.
extern const char kDiceMigrationDialogLastShownTime[];

// Tracks whether the user has been migrated to explicitly signed-in state
// following the DICe migration flow.
extern const char kDiceMigrationMigrated[];

// Stores the backup of the prefs updated during the DICe migration. This allows
// restoring the prefs to their previous state in case the feature flag is
// disabled.
extern const char kDiceMigrationBackup[];

// This tracks whether the user had DICe migration reverted due to feature flag
// being disabled.
extern const char kDiceMigrationRestoredFromBackup[];

class DiceMigrationService : public KeyedService,
                             public views::WidgetObserver,
                             public signin::IdentityManager::Observer {
 public:
  // The maximum number of times the dialog can be shown.
  static const int kMaxDialogShownCount;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAcceptButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(DialogCloseReason)
  enum class DialogCloseReason {
    kUnspecified = 0,  // The dialog was closed without a specific reason, most
                       // likely to be a browser shutdown.
    kAccepted = 1,
    kCancelled = 2,
    kClosed = 3,
    kEscKeyPressed = 4,
    kPrimaryAccountCleared = 5,
    kPrimaryAccountChanged = 6,
    kSyncTurnedOn = 7,
    kAvatarButtonClicked = 8,
    kServiceDestroyed = 9,
    kMaxValue = kServiceDestroyed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:DiceMigrationDialogCloseReason)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(DialogNotShownReason)
  enum class DialogNotShownReason {
    kNotEligible = 0,
    kMaxShownCountReached = 1,
    kMinTimeBetweenDialogsNotPassed = 2,
    kManagedAccount = 3,
    kErrorFetchingAccountManagedStatus = 4,
    kPrimaryAccountChanged = 5,
    kPrimaryAccountCleared = 6,
    kSyncTurnedOn = 7,
    kBrowserInstanceUnavailable = 8,
    kAvatarButtonUnavailable = 9,
    kServiceDestroyed = 10,
    kMaxValue = kServiceDestroyed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:DiceMigrationDialogNotShownReason)

  // `task_runner` is used to schedule the dialog trigger timer during testing.
  explicit DiceMigrationService(Profile* profile,
                                scoped_refptr<base::SingleThreadTaskRunner>
                                    task_runner_for_testing = nullptr);
  DiceMigrationService(const DiceMigrationService&) = delete;
  DiceMigrationService& operator=(const DiceMigrationService&) = delete;
  ~DiceMigrationService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void RevertDiceMigration(PrefService* prefs);

  views::Widget* GetDialogWidgetForTesting();

  base::OneShotTimer& GetDialogTriggerTimerForTesting();

 private:
  class AvatarButtonObserver;

  // `views::WidgetObserver`:
  void OnWidgetDestroying(views::Widget* widget) override;

  // `signin::IdentityManager::Observer`:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  void OnTimerFinishOrAccountManagedStatusKnown();

  void StopTimerOrCloseDialog(DialogCloseReason reason);

  std::optional<DialogNotShownReason> ShouldStartDialogTriggerTimer();

  // Shows the Dice migration offer dialog if the user is eligible for it.
  std::optional<DialogNotShownReason>
  ShowDiceMigrationOfferDialogIfUserEligible();

  // Getters/setter for the dialog shown count and last shown time prefs.
  int GetDialogShownCount() const;
  base::Time GetDialogLastShownTime() const;
  void UpdateDialogShownCountAndTime();

  // Invoked when `browser_` is closed.
  void BrowserDidClose(BrowserWindowInterface* browser);

  bool ForceMigrateUserIfEligible();

  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // The account info of the account taken into account here.
  CoreAccountInfo primary_account_info_;

  // Timer used to trigger the dialog after a grace period.
  base::OneShotTimer dialog_trigger_timer_;
  std::unique_ptr<signin::AccountManagedStatusFinder>
      account_managed_status_finder_;

  raw_ptr<views::Widget> dialog_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};

  // The browser instance that was used to show the dialog.
  base::WeakPtr<Browser> browser_;
  std::optional<base::CallbackListSubscription> browser_close_subscription_;

  // Observes the avatar button to close the dialog when it is clicked.
  std::unique_ptr<AvatarButtonObserver> avatar_button_observer_;

  // This stores the reason why the dialog was manually closed by the service.
  std::optional<DialogCloseReason> dialog_close_reason_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
