// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
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
// IMPORTANT(!): The dialog is considered shown only if the user interacts with
// it, i.e. the user accepts or dismisses the dialog. This is better than just
// tracking when the dialog was actually shown, since the user might have
// dismissed the dialog unknowingly, for example, by closing the browser.
extern const char kDiceMigrationDialogShownCount[];

// Tracks the last time the DICe migration dialog was shown.
// IMPORTANT(!): The dialog is considered shown only if the user interacts with
// it, i.e. the user accepts or dismisses the dialog. This is better than just
// tracking when the dialog was actually shown, since the user might have
// dismissed the dialog unknowingly, for example, by closing the browser.
extern const char kDiceMigrationDialogLastShownTime[];

class DiceMigrationService : public KeyedService,
                             public views::WidgetObserver,
                             public signin::IdentityManager::Observer {
 public:
  // The maximum number of times the dialog can be shown.
  static const int kMaxDialogShownCount;
  // The minimum time between dialogs.
  static const base::TimeDelta kMinTimeBetweenDialogInDays;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAcceptButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);

  explicit DiceMigrationService(Profile* profile);
  DiceMigrationService(const DiceMigrationService&) = delete;
  DiceMigrationService& operator=(const DiceMigrationService&) = delete;
  ~DiceMigrationService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  views::Widget* GetDialogWidgetForTesting();

  base::OneShotTimer& GetDialogTriggerTimerForTesting();

 private:
  // `views::WidgetObserver`:
  void OnWidgetDestroying(views::Widget* widget) override;

  // `signin::IdentityManager::Observer`:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  void OnTimerFinishOrAccountManagedStatusKnown();

  void StopTimerOrCloseDialog();

  // Shows the Dice migration offer dialog if the user is eligible for it.
  void ShowDiceMigrationOfferDialogIfUserEligible();

  // Getters/setter for the dialog shown count and last shown time prefs.
  int GetDialogShownCount() const;
  base::Time GetDialogLastShownTime() const;
  void UpdateDialogShownCountAndTime();

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
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
