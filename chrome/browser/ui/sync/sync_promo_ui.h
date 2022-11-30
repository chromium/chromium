// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_SYNC_PROMO_UI_H_
#define CHROME_BROWSER_UI_SYNC_SYNC_PROMO_UI_H_

class Profile;

// Static helper function useful for sync promos.
class SyncPromoUI {
 public:
  // Returns true if the sync promo should be visible.
  // |profile| is the profile for which the promo would be displayed.
  static bool ShouldShowSyncPromo(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_SYNC_SYNC_PROMO_UI_H_
