// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/scoped_client_impersonation.h"

#include <objbase.h>

#include <ostream>

#include "base/check.h"

ScopedClientImpersonation::ScopedClientImpersonation()
    : result_(::CoGetCallContext(IID_PPV_ARGS(&server_security_))) {
  if (SUCCEEDED(result_)) {
    // Impersonation is not nested, so it is illegal to try to create a new
    // instance while one is already alive. This is prevented, since otherwise
    // the first to destroy their scoper would revert impersonation for all
    // others.
    CHECK(!server_security_->IsImpersonating());
    result_ = server_security_->ImpersonateClient();
    if (FAILED(result_)) {
      server_security_.Reset();
    }
  }
}

ScopedClientImpersonation::~ScopedClientImpersonation() {
  if (server_security_) {
    HRESULT hr = server_security_->RevertToSelf();
    CHECK(SUCCEEDED(hr)) << hr;
  }
}
