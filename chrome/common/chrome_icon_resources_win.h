// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_ICON_RESOURCES_WIN_H_
#define CHROME_COMMON_CHROME_ICON_RESOURCES_WIN_H_

#include "build/branding_buildflags.h"

namespace icon_resources {

// This file contains the indices of icon resources in chrome_exe.rc.

enum {
  // The main application icon is always index 0.
  kApplicationIndex = 0,

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Legacy indices that are no longer used.
  kApplication2Index = 1,
  kApplication3Index = 2,
  kApplication4Index = 3,

  // The Chrome Canary application icon.
  kSxSApplicationIndex = 4,

  // The Chrome App Launcher icon.
  kAppLauncherIndex = 5,

  // The Chrome App Launcher Canary icon.
  kSxSAppLauncherIndex = 6,

  // The Chrome incognito icon.
  kIncognitoIndex = 7,

  // The Chrome Dev application icon.
  kDevApplicationIndex = 8,

  // The Chrome Beta application icon.
  kBetaApplicationIndex = 9,

  // The Chrome html doc icon.
  kHtmlDocIndex = 10,

  // The Chrome PDF doc icon.
  kPDFDocIndex = 11,

#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The Chromium App Launcher icon.
  kAppLauncherIndex = 1,

  // The Chromium incognito icon.
  kIncognitoIndex = 2,

  // The Chromium html doc icon.
  kHtmlDocIndex = 3,

  // The Chromium PDF doc icon.
  kPDFDocIndex = 4,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

}  // namespace icon_resources

#endif  // CHROME_COMMON_CHROME_ICON_RESOURCES_WIN_H_
