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

class Browser;
class Profile;
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
namespace views {
class Widget;
}  // namespace views

// Tracks the number of times the DICe migration dialog has been shown.
extern const char kDiceMigrationDialogShownCount[];

class DiceMigrationService : public KeyedService, public views::WidgetObserver {
 public:
  // The maximum number of times the dialog can be shown.
  static const int kMaxDialogShownCount;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAcceptButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);

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

  int GetDialogShownCount() const;
  void IncrementDialogShownCount();

  raw_ptr<Profile> profile_ = nullptr;

  raw_ptr<views::Widget> dialog_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      dialog_widget_observation_{this};
  // The browser instance that was used to show the dialog.
  base::WeakPtr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_DICE_MIGRATION_SERVICE_H_
