// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_constants.h"

namespace installer {

// Elements that make up install paths.
const wchar_t kChromeArchive[] = L"chrome.7z";
const wchar_t kChromeCompressedArchive[] = L"chrome.packed.7z";
const wchar_t kVisualElements[] = L"VisualElements";
const wchar_t kVisualElementsManifest[] = L"chrome.VisualElementsManifest.xml";

// Sub directory of install source package under install temporary directory.
const wchar_t kInstallSourceDir[] = L"source";
const wchar_t kInstallSourceChromeDir[] = L"Chrome-bin";

const wchar_t kMediaPlayerRegPath[] =
    L"Software\\Microsoft\\MediaPlayer\\ShimInclusionList";

// Local State preference names.
const char kUninstallMetricsName[] = "uninstall_metrics";

const char kCourgette[] = "courgette";
const char kBsdiff[] = "bsdiff";
#if BUILDFLAG(ZUCCHINI)
const char kZucchini[] = "zucchini";
#endif  // BUILDFLAG(ZUCCHINI)

namespace switches {

// Setting this will delay the operation of setup by the specified number of
// seconds.  This is used when changing the DisplayVersion registry value
// only after some time has passed, called by the MSI installer.
const char kDelay[] = "delay";

// Set the MSI-managed DisplayVersion in the registry to match Chrome's real
// version number. The parameter to this option specifies the product-id in
// the registry under HKLM.
const char kSetDisplayVersionProduct[] = "set-display-version-product";
const char kSetDisplayVersionValue[] = "set-display-version-value";

// Run setup.exe to conduct a post-update experiment.
const char kUserExperiment[] = "user-experiment";

}  // namespace switches

}  // namespace installer
