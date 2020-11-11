// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/mac/keystone_glue.h"

#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
#include "chrome/browser/updater/browser_updater_client_util.h"
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)

InstalledAndCriticalVersion GetInstalledVersion() {
#if BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
  return InstalledAndCriticalVersion(
      base::Version(CurrentlyInstalledVersion()));
#else
  return InstalledAndCriticalVersion(base::Version(
      base::UTF16ToASCII(keystone_glue::CurrentlyInstalledVersion())));
#endif  // BUILDFLAG(ENABLE_CHROMIUM_UPDATER)
}
