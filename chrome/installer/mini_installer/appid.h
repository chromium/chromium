// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_APPID_H_
#define CHROME_INSTALLER_MINI_INSTALLER_APPID_H_

#include "build/branding_buildflags.h"

// The appid included by the mini_installer.
namespace google_update {

extern const wchar_t kAppGuid[];

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
extern const wchar_t kBetaAppGuid[];
extern const wchar_t kDevAppGuid[];
extern const wchar_t kSxSAppGuid[];
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace google_update

#endif  // CHROME_INSTALLER_MINI_INSTALLER_APPID_H_
