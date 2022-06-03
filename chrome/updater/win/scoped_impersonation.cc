// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/scoped_impersonation.h"

#include "base/logging.h"
#include "chrome/updater/win/win_util.h"

namespace updater {

HRESULT ScopedImpersonation::Impersonate(HANDLE token) {
  if (!token)
    return E_FAIL;

  result_ = ::ImpersonateLoggedOnUser(token) ? S_OK : HRESULTFromLastError();
  DCHECK_EQ(result_, S_OK);
  return result_;
}

ScopedImpersonation::~ScopedImpersonation() {
  if (result_ != S_OK)
    return;

  CHECK(::RevertToSelf());
}

}  // namespace updater
