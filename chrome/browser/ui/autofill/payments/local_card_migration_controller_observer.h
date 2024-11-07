// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_

#include "base/observer_list_types.h"

// The observer interface that listens for events in local card migration
// related controllers.
class LocalCardMigrationControllerObserver: public base::CheckedObserver {
 public:
  // Called when the user declined the offer dialog, navigated away with
  // feedback credit card icon or finished with the feedback dialog.
  virtual void OnMigrationNoLongerAvailable() = 0;
  // Called after the user clicked the save button. Will trigger the
  // credit card icon animation.
  virtual void OnMigrationStarted() = 0;

  enum class LocalCardMigrationControllerSource {
    kBubbleController,
    kDialogContoller,
  };
  // Called during source destruction to reset scoped observation stored in the
  // observer.
  virtual void OnSourceDestruction(
      LocalCardMigrationControllerSource source) = 0;

 protected:
  ~LocalCardMigrationControllerObserver() override = default;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_
