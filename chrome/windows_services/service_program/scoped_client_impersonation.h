// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SCOPED_CLIENT_IMPERSONATION_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SCOPED_CLIENT_IMPERSONATION_H_

#include <objidl.h>
#include <wrl/client.h>

#include "base/win/windows_types.h"

// A helper for temporarily impersonating the client.
class ScopedClientImpersonation {
 public:
  ScopedClientImpersonation();
  ScopedClientImpersonation(const ScopedClientImpersonation&) = delete;
  ScopedClientImpersonation& operator=(const ScopedClientImpersonation&) =
      delete;
  ~ScopedClientImpersonation();

  // Returns true if impersonation succeeded.
  bool is_valid() const { return SUCCEEDED(result_); }

  // Returns the HRESULT in the event that impersonation failed.
  HRESULT result() const { return result_; }

 private:
  // Non-null when impersonating.
  Microsoft::WRL::ComPtr<IServerSecurity> server_security_;

  // The last result code.
  HRESULT result_;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SCOPED_CLIENT_IMPERSONATION_H_
