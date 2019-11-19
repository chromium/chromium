// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_

// The observer interface that listens for events in local card migration
// related controllers.
class LocalCardMigrationControllerObserver {
 public:
  // Called when the user declined the offer dialog, navigated away with
  // feedback credit card icon or finished with the feedback dialog.
  virtual void OnMigrationNoLongerAvailable() = 0;
  // Called after the user clicked the save button. Will trigger the
  // credit card icon animation.
  virtual void OnMigrationStarted() = 0;

 protected:
  virtual ~LocalCardMigrationControllerObserver() = default;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_
