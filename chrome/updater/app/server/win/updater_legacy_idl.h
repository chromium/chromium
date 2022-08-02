// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This header is used to encapsulate choosing between including either the
// chrome-branded or the open-source `updater_legacy_idl` headers. This reduces
// the `BUILDFLAG(GOOGLE_CHROME_BRANDING)` clutter.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_UPDATER_LEGACY_IDL_H_
#define CHROME_UPDATER_APP_SERVER_WIN_UPDATER_LEGACY_IDL_H_

#include "build/branding_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/updater/app/server/win/updater_legacy_idl_chrome_branded.h"
#else
#include "chrome/updater/app/server/win/updater_legacy_idl_open_source.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#endif  // CHROME_UPDATER_APP_SERVER_WIN_UPDATER_LEGACY_IDL_H_
