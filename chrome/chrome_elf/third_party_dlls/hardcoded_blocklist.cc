// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/hardcoded_blocklist.h"

namespace third_party_dlls {

namespace {

// Max size of the DLL blocklist.
constexpr size_t kDllBlocklistMaxSize = 64u;

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the browser. DLLs should only be added to this list
// if there is nothing else Chrome can do to prevent those crashes.
// For more information about how this list is generated, and how to get off
// of it, see:
// https://sites.google.com/a/chromium.org/dev/Home/third-party-developers
const char* const kDllBlocklist[kDllBlocklistMaxSize] = {
    "949ba8b6a9.dll",           // Coupon Time.
    "activedetect32.dll",       // Lenovo One Key Theater.
                                // See crbug.com/379218.
    "activedetect64.dll",       // Lenovo One Key Theater.
    "bitguard.dll",             // Unknown (suspected malware).
    "bsvc.dll",                 // Unknown (suspected adware).
    "chrmxtn.dll",              // Unknown (keystroke logger).
    "cplushook.dll",            // Unknown (suspected malware).
    "crdli.dll",                // Linkury Inc.
    "crdli64.dll",              // Linkury Inc.
    "datamngr.dll",             // Unknown (suspected adware).
    "dpinterface32.dll",        // Unknown (suspected adware).
    "explorerex.dll",           // Unknown (suspected adware).
    "hk.dll",                   // Unknown (keystroke logger).
    "libapi2hook.dll",          // V-Bates.
    "libinject.dll",            // V-Bates.
    "libinject2.dll",           // V-Bates.
    "libredir2.dll",            // V-Bates.
    "libsvn_tsvn32.dll",        // TortoiseSVN.
    "libwinhook.dll",           // V-Bates.
    "lmrn.dll",                 // Unknown.
    "minisp.dll",               // Unknown (suspected malware).
    "minisp32.dll",             // Unknown (suspected malware).
    "offerswizarddll.dll",      // Unknown (suspected adware).
    "safetynut.dll",            // Unknown (suspected adware).
    "smdmf.dll",                // Unknown (suspected adware).
    "spappsv32.dll",            // Unknown (suspected adware).
    "systemk.dll",              // Unknown (suspected adware).
    "vntsrv.dll",               // Virtual New Tab by APN LLC.
    "wajam_goblin_64.dll",      // Wajam Internet Technologies.
    "wajam_goblin.dll",         // Wajam Internet Technologies.
    "windowsapihookdll32.dll",  // Lenovo One Key Theater.
                                // See crbug.com/379218.
    "windowsapihookdll64.dll",  // Lenovo One Key Theater.
    "virtualcamera.ax",         // %PROGRAMFILES%\ASUS\VirtualCamera.
                                // See crbug.com/422522.
    "ycwebcamerasource.ax",     // CyberLink Youcam, crbug.com/424159
    // Keep this null pointer here to mark the end of the list.
    nullptr,
};

}  // namespace

bool DllMatch(const std::string& module_name) {
  if (module_name.empty())
    return false;

  for (int i = 0; kDllBlocklist[i] != nullptr; ++i) {
    if (strcmp(module_name.c_str(), kDllBlocklist[i]) == 0)
      return true;
  }

  return false;
}

}  // namespace third_party_dlls
