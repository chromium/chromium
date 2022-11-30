// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_MAC_GCAPI_H_
#define CHROME_INSTALLER_GCAPI_MAC_GCAPI_H_

// Error conditions for GoogleChromeCompatibilityCheck().
#define GCCC_ERROR_ALREADYPRESENT (1 << 0)
#define GCCC_ERROR_ACCESSDENIED (1 << 1)
#define GCCC_ERROR_OSNOTSUPPORTED (1 << 2)
#define GCCC_ERROR_ALREADYOFFERED (1 << 3)
#define GCCC_ERROR_INTEGRITYLEVEL (1 << 4)

#ifdef __cplusplus
extern "C" {
#endif

// This function returns nonzero if Google Chrome should be offered.
// If the return value is 0, |reasons| explains why.  If you don't care for the
// reason, you can pass nullptr for |reasons|.
int GoogleChromeCompatibilityCheck(unsigned* reasons);

// This function installs Google Chrome in the application folder and optionally
// sets up the brand code and master prefs.
// |source_path| Path to an uninstalled Google Chrome.app directory, for example
//               in a mounted dmg, in file system representation.
// |brand_code| If not nullptr, a string containing the brand code Google Chrome
//              should use. Has no effect if Google Chrome has an embedded brand
//              code. Overwrites existing brand files.
// |master_prefs_contents| If not nullptr, the _contents_ of a master prefs file
//                         Google Chrome should use. This is not a path.
//                         Overwrites existing master pref files.
// Returns nonzero if Google Chrome was successfully copied. If copying
// succeeded but writing of master prefs, brand code, or other noncrucial
// setup tasks fail, this still returns nonzero.
// Returns 0 if the installation failed, for example if Google Chrome was
// already installed, or no disk space was left.
int InstallGoogleChrome(const char* source_path,
                        const char* brand_code,
                        const char* master_prefs_contents,
                        unsigned master_prefs_contents_size);

// This function launches Google Chrome after a successful install, or it does
// a best-effort search to launch an existing installation if
// InstallGoogleChrome() returned GCCC_ERROR_ALREADYPRESENT.
int LaunchGoogleChrome();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CHROME_INSTALLER_GCAPI_MAC_GCAPI_H_
