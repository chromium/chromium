// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares constants that describe specifics of a Chromium-based
// browser's branding and modes of installation.
//
// A browser's brand comprises all identifying markings that distinguish it from
// a browser produced by another party. Chromium has remnants of both the
// Chromium and Google Chrome brands.
//
// Each brand defines one primary install mode for the browser. A brand may
// additionally define one or more secondary install modes (e.g., Google
// Chrome's SxS mode for the canary channel). Install modes are described by
// compile-time instantiations of the `InstallConstants` struct in a brand's
// `kInstallModes` array. Index 0 of this array always describes the primary
// install mode. All other entries describe secondary modes, which are
// distinguished by an install suffix (e.g., " SxS") that appears in file and
// registry paths. A given machine may have any combination of a brand's modes
// installed at the same time (e.g., primary only, one or more secondary, or
// even primary and all secondary modes). Parallel installs of a brand's modes
// (e.g., Google Chrome and Google Chrome SxS (canary) installed on the same
// machine) store user data in distinct directories. Administrative policies, on
// the other hand, apply to all of a brand's install modes (e.g., the
// AlwaysOpenPdfExternally group policy setting applies to both Google Chrome
// and Google Chrome SxS.

#ifndef CHROME_INSTALL_STATIC_INSTALL_MODES_H_
#define CHROME_INSTALL_STATIC_INSTALL_MODES_H_

#include <string>

#include "build/branding_buildflags.h"
#include "chrome/install_static/install_constants.h"

// Include the brand-specific values. Each of these must define:
// - enum InstallConstantIndex: named indices of the brand's kInstallModes
//   array.
// - NUM_INSTALL_MODES: the total number of modes (i.e., the number of items in
//   kInstallModes.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/install_static/google_chrome_install_modes.h"
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
#include "chrome/install_static/google_chrome_for_testing_install_modes.h"
#else
#include "chrome/install_static/chromium_install_modes.h"
#endif

namespace install_static {

// The brand-specific company name to be included as a component of the install
// and user data directory paths. May be empty if no such dir is to be used.
extern const wchar_t kCompanyPathName[];

// The brand-specific product name to be included as a component of the install
// and user data directory paths.
extern const wchar_t kProductPathName[];

// The length, in characters, of kProductPathName not including the terminator.
extern const size_t kProductPathNameLength;

// The brand-specific safe browsing client name.
extern const char kSafeBrowsingName[];

// A brand's collection of install modes.
extern const InstallConstants kInstallModes[];

// The following convenience functions behave conditionally on whether or not
// the brand uses Chrome's integration with Google Update. For brands that do
// not (e.g., Chromium), they return something like "Software\Chromium" or
// "Software\Chromium Binaries". Otherwise, for brands that do integrate with
// Google Update, they return something like
// "Software\Google\Update\ClientState{Medium}\<guid>" where "<guid>" is either
// |mode|'s appguid.
std::wstring GetClientsKeyPath(const wchar_t* app_guid);
std::wstring GetClientStateKeyPath(const wchar_t* app_guid);
std::wstring GetClientStateMediumKeyPath(const wchar_t* app_guid);

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_MODES_H_
