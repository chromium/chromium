// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_FAKE_SESSION_MANAGER_DELEGATE_H_
#define COMPONENTS_SESSION_MANAGER_CORE_FAKE_SESSION_MANAGER_DELEGATE_H_

#include "components/session_manager/core/session_manager_delegate.h"

namespace session_manager {

// A fake implementation of SessionManagerDelegate. This allows testing
// components that depend on SessionManager without requiring the real
// implementation from //chrome to avoid violating dependencies.
class FakeSessionManagerDelegate : public SessionManagerDelegate {
 public:
  FakeSessionManagerDelegate();
  FakeSessionManagerDelegate(const FakeSessionManagerDelegate&) = delete;
  FakeSessionManagerDelegate& operator=(const FakeSessionManagerDelegate&) =
      delete;

  ~FakeSessionManagerDelegate() override;

  // session_manager::SessionManagerDelegate override:
  void RequestSignOut() override;

  // Returns the number of RequestSignOut() calls.
  int request_sign_out_count() const;

 private:
  int request_sign_out_count_ = 0;
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_FAKE_SESSION_MANAGER_DELEGATE_H_
