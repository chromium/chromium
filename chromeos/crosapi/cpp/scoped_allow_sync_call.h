// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_SCOPED_ALLOW_SYNC_CALL_H_
#define CHROMEOS_CROSAPI_CPP_SCOPED_ALLOW_SYNC_CALL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

class ChromePasswordManagerClient;
class ChromePasswordReuseDetectionManagerClient;

namespace crosapi {

// Chrome generally disallows sync IPC calls. Crosapi allows a small number of
// exceptions to support cross-platform code where other platforms all provide a
// synchronous implementation of a particular API. Use this sparingly.
class COMPONENT_EXPORT(CROSAPI) ScopedAllowSyncCall {
 private:
  // Consumers of this class must be explicitly added as a friend.
  friend class ::ChromePasswordManagerClient;
  friend class ::ChromePasswordReuseDetectionManagerClient;

  ScopedAllowSyncCall();
  ScopedAllowSyncCall(const ScopedAllowSyncCall&) = delete;
  ScopedAllowSyncCall& operator=(const ScopedAllowSyncCall&) = delete;
  ~ScopedAllowSyncCall();

  mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_allow_;
};

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_SCOPED_ALLOW_SYNC_CALL_H_
