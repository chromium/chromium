// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_H_
#define CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_H_

#include <windows.h>

#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "base/win/wincrypt_shim.h"

namespace chrome_cleaner {

// Enumerate the contexts of the given certificate store.
class CertificateEnumerator {
 public:
  explicit CertificateEnumerator(HCERTSTORE store_handle) {
    store_handle_ = store_handle;
  }

  ~CertificateEnumerator() {
    if (context_)
      ::CertFreeCertificateContext(context_);
  }

  bool Next() {
    context_ = ::CertEnumCertificatesInStore(store_handle_, context_);
    return context_ != nullptr;
  }

  PCCERT_CONTEXT context() const { return context_; }

 private:
  HCERTSTORE store_handle_;
  PCCERT_CONTEXT context_ = nullptr;
};

// Set |medium_integrity_token| with a medium integrity level token duplicated
// from the current process token. Return false on failures or if called when
// running on a Windows version before Vista.
bool GetMediumIntegrityToken(base::win::ScopedHandle* medium_integrity_token);

// Convert a GUID to its textual representation. |output| receives the textual
// representation.
void GUIDToString(const GUID&, base::string16* output);

// Set the current process to background mode.
void SetBackgroundMode();

// Return whether Windows is performing registry key redirection for Windows
// on Windows for the current process.
// Do not use this value as sign of x64 architecture, as it true only for
// 32-bit processes running on 64-bit OS.
bool IsWowRedirectionActive();

// Return whether this version of Windows uses x64 processor architecture.
bool IsX64Architecture();

// Return whether the current process is 64-bit.
bool IsX64Process();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_SYSTEM_UTIL_H_
