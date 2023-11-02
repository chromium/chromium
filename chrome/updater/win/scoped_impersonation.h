// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SCOPED_IMPERSONATION_H_
#define CHROME_UPDATER_WIN_SCOPED_IMPERSONATION_H_

#include <windows.h>

namespace updater {

class ScopedImpersonation {
 public:
  ScopedImpersonation() = default;
  ScopedImpersonation(const ScopedImpersonation&) = delete;
  ScopedImpersonation& operator=(const ScopedImpersonation&) = delete;

  ~ScopedImpersonation();

  HRESULT Impersonate(HANDLE session);

 private:
  HRESULT result_ = E_FAIL;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SCOPED_IMPERSONATION_H_
