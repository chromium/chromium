// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_H_

namespace autofill {

// The cross-platform interface which displays the intermediate bubble
// for browser-stored local card migration flow.
class LocalCardMigrationBubble {
 public:
  // Called from controller to shut down the bubble and prevent any further
  // action.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_BUBBLE_H_
