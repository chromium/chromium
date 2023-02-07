// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/nss_decryptor_win.h"

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"

namespace {

// A helper class whose destructor calls SetDllDirectory(nullptr) to undo the
// effects of a previous SetDllDirectory call.
class ScopedSetDllDirectoryCaller {
 public:
  ScopedSetDllDirectoryCaller() = default;
  ~ScopedSetDllDirectoryCaller() { ::SetDllDirectory(nullptr); }
};

}  // namespace

// static
const wchar_t NSSDecryptor::kNSS3Library[] = L"nss3.dll";
const wchar_t NSSDecryptor::kPLDS4Library[] = L"plds4.dll";
const wchar_t NSSDecryptor::kNSPR4Library[] = L"nspr4.dll";

bool NSSDecryptor::Init(const base::FilePath& dll_path,
                        const base::FilePath& db_path) {
  // We call SetDllDirectory to work around a Purify bug (GetModuleHandle
  // fails inside Purify under certain conditions).

  if (!::SetDllDirectory(dll_path.value().c_str())) {
    return false;
  }

  ScopedSetDllDirectoryCaller caller;
  nss3_dll_ = LoadLibrary(kNSS3Library);
  if (!nss3_dll_) {
    return false;
  }

  HMODULE plds4_dll = GetModuleHandle(kPLDS4Library);
  HMODULE nspr4_dll = GetModuleHandle(kNSPR4Library);

  // On Firefox 22 and higher, NSPR is part of nss3.dll rather than separate
  // DLLs.
  if (!plds4_dll) {
    plds4_dll = nss3_dll_;
    nspr4_dll = nss3_dll_;
  }
  return InitNSS(db_path, plds4_dll, nspr4_dll);
}

NSSDecryptor::NSSDecryptor()
    : NSS_Init(NULL),
      NSS_Shutdown(NULL),
      PK11_GetInternalKeySlot(NULL),
      PK11_FreeSlot(NULL),
      PK11_Authenticate(NULL),
      PK11SDR_Decrypt(NULL),
      SECITEM_FreeItem(NULL),
      PL_ArenaFinish(NULL),
      PR_Cleanup(NULL),
      nss3_dll_(NULL),
      softokn3_dll_(NULL),
      is_nss_initialized_(false) {}

NSSDecryptor::~NSSDecryptor() {
  Free();
}

bool NSSDecryptor::InitNSS(const base::FilePath& db_path,
                           base::NativeLibrary plds4_dll,
                           base::NativeLibrary nspr4_dll) {
  // Gets the function address.
  NSS_Init = (NSSInitFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "NSS_Init");
  NSS_Shutdown = (NSSShutdownFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "NSS_Shutdown");
  PK11_GetInternalKeySlot = (PK11GetInternalKeySlotFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_,
                                                "PK11_GetInternalKeySlot");
  PK11_FreeSlot = (PK11FreeSlotFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "PK11_FreeSlot");
  PK11_Authenticate = (PK11AuthenticateFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "PK11_Authenticate");
  PK11SDR_Decrypt = (PK11SDRDecryptFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "PK11SDR_Decrypt");
  SECITEM_FreeItem = (SECITEMFreeItemFunc)
      base::GetFunctionPointerFromNativeLibrary(nss3_dll_, "SECITEM_FreeItem");
  PL_ArenaFinish = (PLArenaFinishFunc)
      base::GetFunctionPointerFromNativeLibrary(plds4_dll, "PL_ArenaFinish");
  PR_Cleanup = (PRCleanupFunc)
      base::GetFunctionPointerFromNativeLibrary(nspr4_dll, "PR_Cleanup");

  if (NSS_Init == NULL || NSS_Shutdown == NULL ||
      PK11_GetInternalKeySlot == NULL || PK11_FreeSlot == NULL ||
      PK11_Authenticate == NULL || PK11SDR_Decrypt == NULL ||
      SECITEM_FreeItem == NULL || PL_ArenaFinish == NULL ||
      PR_Cleanup == NULL) {
    Free();
    return false;
  }

  SECStatus result = NSS_Init(base::SysWideToNativeMB(db_path.value()).c_str());
  if (result != SECSuccess) {
    Free();
    return false;
  }

  is_nss_initialized_ = true;
  return true;
}

void NSSDecryptor::Free() {
  if (is_nss_initialized_) {
    NSS_Shutdown();
    PL_ArenaFinish();
    PR_Cleanup();
    is_nss_initialized_ = false;
  }
  if (softokn3_dll_ != NULL)
    base::UnloadNativeLibrary(softokn3_dll_);
  if (nss3_dll_ != NULL)
    base::UnloadNativeLibrary(nss3_dll_);
  NSS_Init = NULL;
  NSS_Shutdown = NULL;
  PK11_GetInternalKeySlot = NULL;
  PK11_FreeSlot = NULL;
  PK11_Authenticate = NULL;
  PK11SDR_Decrypt = NULL;
  SECITEM_FreeItem = NULL;
  PL_ArenaFinish = NULL;
  PR_Cleanup = NULL;
  nss3_dll_ = NULL;
  softokn3_dll_ = NULL;
}
