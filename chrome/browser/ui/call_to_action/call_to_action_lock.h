// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CALL_TO_ACTION_CALL_TO_ACTION_LOCK_H_
#define CHROME_BROWSER_UI_CALL_TO_ACTION_CALL_TO_ACTION_LOCK_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// A feature which wants to show window level call to action UI  should call
// CallToActionLock::AcquireLock and keep alive the instance of
// ScopedCallToActionLock for the duration of the window-modal UI.
class ScopedCallToActionLock {
 public:
  ScopedCallToActionLock() = default;
  virtual ~ScopedCallToActionLock() = default;
};

// Features that want to show a window level call to action UI can be mutually
// exclusive. Before gating on call to action UI first check
// `CanAcquireLock`. Then call AcquireLock() and keep
// `ScopedCallToActionLock` alive to prevent other features from showing
// window level call to action Uis.
class CallToActionLock {
 public:
  DECLARE_USER_DATA(CallToActionLock);

  explicit CallToActionLock(BrowserWindowInterface* browser_window);
  CallToActionLock(const CallToActionLock&) = delete;
  CallToActionLock& operator=(const CallToActionLock&) = delete;
  virtual ~CallToActionLock();

  static CallToActionLock* From(BrowserWindowInterface* browser_window);

  virtual bool CanAcquireLock() const;
  virtual std::unique_ptr<ScopedCallToActionLock> AcquireLock();

  void OnScopedCallToActionLockDestroyed();

 private:
  bool showing_call_to_action_ = false;
  ui::ScopedUnownedUserData<CallToActionLock> scoped_user_data_;

  base::WeakPtrFactory<CallToActionLock> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_CALL_TO_ACTION_CALL_TO_ACTION_LOCK_H_
