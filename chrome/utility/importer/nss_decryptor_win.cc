// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/nss_decryptor_win.h"

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"

namespace {

typedef BOOL (WINAPI* SetDllDirectoryFunc)(LPCTSTR lpPathName);

// A helper class whose destructor calls SetDllDirectory(NULL) to undo the
// effects of a previous SetDllDirectory call.
class SetDllDirectoryCaller {
 public:
  SetDllDirectoryCaller() : func_(NULL) {}

  ~SetDllDirectoryCaller() {
    if (func_)
      func_(NULL);
  }

  // Sets the SetDllDirectory function pointer to activates this object.
  void set_func(SetDllDirectoryFunc func) { func_ = func; }

 private:
  SetDllDirectoryFunc func_;
};

}  // namespace

// static
const wchar_t NSSDecryptor::kNSS3Library[] = L"nss3.dll";
const wchar_t NSSDecryptor::kSoftokn3Library[] = L"softokn3.dll";
const wchar_t NSSDecryptor::kPLDS4Library[] = L"plds4.dll";
const wchar_t NSSDecryptor::kNSPR4Library[] = L"nspr4.dll";

bool NSSDecryptor::Init(const base::FilePath& dll_path,
                        const base::FilePath& db_path) {
  // We call SetDllDirectory to work around a Purify bug (GetModuleHandle
  // fails inside Purify under certain conditions).  SetDllDirectory only
  // exists on Windows XP SP1 or later, so we look up its address at run time.
  HMODULE kernel32_dll = GetModuleHandle(L"kernel32.dll");
  if (kernel32_dll == NULL)
    return false;
  SetDllDirectoryFunc set_dll_directory =
      (SetDllDirectoryFunc)GetProcAddress(kernel32_dll, "SetDllDirectoryW");
  SetDllDirectoryCaller caller;

  if (set_dll_directory != NULL) {
    if (!set_dll_directory(dll_path.value().c_str()))
      return false;
    caller.set_func(set_dll_directory);
    nss3_dll_ = LoadLibrary(kNSS3Library);
    if (nss3_dll_ == NULL)
      return false;
  } else {
    // Fall back on LoadLibraryEx if SetDllDirectory isn't available.  We
    // actually prefer this method because it doesn't change the DLL search
    // path, which is a process-wide property.
    base::FilePath path = dll_path.Append(kNSS3Library);
    nss3_dll_ = LoadLibraryEx(path.value().c_str(), NULL,
                              LOAD_WITH_ALTERED_SEARCH_PATH);
    if (nss3_dll_ == NULL)
      return false;

    // Firefox 2 uses NSS 3.11.  Firefox 3 uses NSS 3.12.  NSS 3.12 has two
    // changes in its DLLs:
    // 1. nss3.dll is not linked with softokn3.dll at build time, but rather
    //    loads softokn3.dll using LoadLibrary in NSS_Init.
    // 2. softokn3.dll has a new dependency sqlite3.dll.
    // NSS_Init's LoadLibrary call has trouble finding sqlite3.dll.  To help
    // it out, we preload softokn3.dll using LoadLibraryEx with the
    // LOAD_WITH_ALTERED_SEARCH_PATH flag.  This helps because LoadLibrary
    // doesn't load a DLL again if it's already loaded.  This workaround is
    // harmless for NSS 3.11.
    path = base::FilePath(dll_path).Append(kSoftokn3Library);
    softokn3_dll_ = LoadLibraryEx(path.value().c_str(), NULL,
                                  LOAD_WITH_ALTERED_SEARCH_PATH);
    if (softokn3_dll_ == NULL) {
      Free();
      return false;
    }
  }
  HMODULE plds4_dll = GetModuleHandle(kPLDS4Library);
  HMODULE nspr4_dll = GetModuleHandle(kNSPR4Library);

  // On Firefox 22 and higher, NSPR is part of nss3.dll rather than separate
  // DLLs.
  if (plds4_dll == NULL) {
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
