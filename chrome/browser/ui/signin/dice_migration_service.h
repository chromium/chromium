// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
#define CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget_observer.h"

class Profile;
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
namespace views {
class Widget;
}  // namespace views

class DiceMigrationService : public KeyedService, public views::WidgetObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAcceptButtonElementId);

  explicit DiceMigrationService(Profile* profile);
  DiceMigrationService(const DiceMigrationService&) = delete;
  DiceMigrationService& operator=(const DiceMigrationService&) = delete;
  ~DiceMigrationService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Shows the Dice migration offer dialog if the user is eligible for it.
  void ShowDiceMigrationOfferDialogIfUserEligible();

  // Returns true if the Dice migration offer dialog is currently showing.
  bool IsDialogShowing();

  views::Widget* GetDialogWidgetForTesting();

 private:
  // `views::WidgetObserver`:
  void OnWidgetDestroying(views::Widget* widget) override;

  raw_ptr<Profile> profile_ = nullptr;

  raw_ptr<views::Widget> dialog_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
