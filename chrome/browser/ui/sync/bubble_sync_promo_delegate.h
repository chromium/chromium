// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_BUBBLE_SYNC_PROMO_DELEGATE_H_
#define CHROME_BROWSER_UI_SYNC_BUBBLE_SYNC_PROMO_DELEGATE_H_

struct AccountInfo;

// Delegate for the bubble sync promo view.
class BubbleSyncPromoDelegate {
 public:
  virtual ~BubbleSyncPromoDelegate() {}

  // Informs the delegate to enable sync for |account| or to present
  // the browser sign-in page if |account| is empty.
  // |is_default_promo_account| is true if |account| corresponds to the default
  // account in the promo. It is ignored if |account| is empty.
  virtual void OnEnableSync(const AccountInfo& account) = 0;
};

#endif  // CHROME_BROWSER_UI_SYNC_BUBBLE_SYNC_PROMO_DELEGATE_H_
