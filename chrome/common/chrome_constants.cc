// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_constants.h"

#include "build/build_config.h"
#include "chrome/common/chrome_version.h"

#define FPL FILE_PATH_LITERAL

namespace chrome {

const char kChromeVersion[] = CHROME_VERSION_STRING;

// The following should not be used for UI strings; they are meant
// for system strings only. UI changes should be made in the GRD.
//
// There are four constants used to locate the executable name and path:
//
//     kBrowserProcessExecutableName
//     kHelperProcessExecutableName
//     kBrowserProcessExecutablePath
//     kHelperProcessExecutablePath
//
// In one condition, our tests will be built using the Chrome branding
// though we want to actually execute a Chromium branded application.
// This happens for the reference build on Mac.  To support that case,
// we also include a Chromium version of each of the four constants and
// in the UITest class we support switching to that version when told to
// do so.

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kBrowserProcessExecutableName[] =
    FPL("chrome.exe");
const base::FilePath::CharType kHelperProcessExecutableName[] =
    FPL("chrome.exe");
#elif BUILDFLAG(IS_MAC)
const base::FilePath::CharType kBrowserProcessExecutableName[] =
    FPL(PRODUCT_FULLNAME_STRING);
const base::FilePath::CharType kHelperProcessExecutableName[] =
    FPL(PRODUCT_FULLNAME_STRING " Helper");
#elif BUILDFLAG(IS_ANDROID)
// NOTE: Keep it synced with the process names defined in AndroidManifest.xml.
const base::FilePath::CharType kBrowserProcessExecutableName[] = FPL("chrome");
const base::FilePath::CharType kHelperProcessExecutableName[] =
    FPL("sandboxed_process");
#elif BUILDFLAG(IS_POSIX)
const base::FilePath::CharType kBrowserProcessExecutableName[] = FPL("chrome");
// Helper processes end up with a name of "exe" due to execing via
// /proc/self/exe.  See bug 22703.
const base::FilePath::CharType kHelperProcessExecutableName[] = FPL("exe");
#endif  // OS_*

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kBrowserProcessExecutablePath[] =
    FPL("chrome.exe");
const base::FilePath::CharType kHelperProcessExecutablePath[] =
    FPL("chrome.exe");
#elif BUILDFLAG(IS_MAC)
const base::FilePath::CharType kBrowserProcessExecutablePath[] =
    FPL(PRODUCT_FULLNAME_STRING ".app/Contents/MacOS/" PRODUCT_FULLNAME_STRING);
const base::FilePath::CharType
    kGoogleChromeForTestingBrowserProcessExecutablePath[] =
        FPL("Google Chrome for Testing.app/Contents/MacOS/Google Chrome for "
            "Testing");
const base::FilePath::CharType kGoogleChromeBrowserProcessExecutablePath[] =
    FPL("Google Chrome.app/Contents/MacOS/Google Chrome");
const base::FilePath::CharType kChromiumBrowserProcessExecutablePath[] =
    FPL("Chromium.app/Contents/MacOS/Chromium");
const base::FilePath::CharType kHelperProcessExecutablePath[] =
    FPL(PRODUCT_FULLNAME_STRING
        " Helper.app/Contents/MacOS/" PRODUCT_FULLNAME_STRING " Helper");
#elif BUILDFLAG(IS_ANDROID)
const base::FilePath::CharType kBrowserProcessExecutablePath[] = FPL("chrome");
const base::FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome");
#elif BUILDFLAG(IS_POSIX)
const base::FilePath::CharType kBrowserProcessExecutablePath[] = FPL("chrome");
const base::FilePath::CharType kHelperProcessExecutablePath[] = FPL("chrome");
#endif  // OS_*

#if BUILDFLAG(IS_MAC)
const base::FilePath::CharType kFrameworkName[] =
    FPL(PRODUCT_FULLNAME_STRING " Framework.framework");
const base::FilePath::CharType kFrameworkExecutableName[] =
    FPL(PRODUCT_FULLNAME_STRING " Framework");
const char kMacHelperSuffixAlerts[] = " (Alerts)";
#endif  // BUILDFLAG(IS_MAC)

}  // namespace chrome

#undef FPL
