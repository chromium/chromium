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
extern const char kDiceMigrationDialogShownCount[];

// Tracks the last time the DICe migration dialog has been shown.
extern const char kDiceMigrationDialogLastShownTime[];

class DiceMigrationService : public KeyedService, public views::WidgetObserver {
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

  void OnTimerFinishOrAccountManagedStatusKnown();

  // Shows the Dice migration offer dialog if the user is eligible for it.
  void ShowDiceMigrationOfferDialogIfUserEligible();

  int GetDialogShownCount() const;
  base::Time GetDialogLastShownTime() const;
  void UpdateDialogShownCountAndTime();

  raw_ptr<Profile> profile_ = nullptr;
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
