// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FIRST_RUN_LACROS_H_
#define CHROME_BROWSER_UI_STARTUP_FIRST_RUN_LACROS_H_

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#error This file should only be included on lacros.
#endif

class Profile;

// Task to run after the FRE is exited, with `proceed` indicating whether it
// should be aborted or resumed.
using ResumeTaskCallback = base::OnceCallback<void(bool proceed)>;

// Returns whether the primary (main) profile first run experience (including
// sync promo) should be opened on startup.
bool ShouldOpenPrimaryProfileFirstRun(Profile* profile);

// Assuming that the primary profile first run experience needs to be opened on
// startup, attempts to complete it silently, in case collecting consent is not
// needed.
// Returns `true` if the FRE was marked finished. If not, `false` will be
// returned and `OpenPrimaryProfileFirstRunIfNeeded()` will need to be
// eventually called to show the visual FRE.
bool TryMarkFirstRunAlreadyFinished(Profile* primary_profile);

// This function takes the user through the browser FRE.
// 1) First, it checks whether the FRE flow can be skipped in the first place.
//    This is the case when sync consent is already given (true for existing
//    users that migrated to lacros) or when enterprise policies forbid the FRE.
//    If so, the call directly 'finishes' the flow (see below).
// 2) Then, it opens the FRE UI (in the profile picker window) and
//    asynchronously 'finishes' the flow (sets a flag in the local prefs) once
//    the user chooses any action on the sync consent screen. If the user exits
//    the FRE UI via the generic 'Close window' affordances, it is interpreted
//    as an intent to exit the app and `callback` will be called with `proceed`
//    set to false. If they exit it via the dedicated options in the flow, it
//    will be considered 'completed' and `callback` will be run with `proceed`
//    set to true. If the FRE flow is exited before the sync consent screen, the
//    flow is considered 'aborted', and can be shown again at the next startup.
// When this method is called again while FRE is in progress, the previous
// callback is aborted (called with false), and is replaced by `callback`.
void OpenPrimaryProfileFirstRunIfNeeded(ResumeTaskCallback callback);

#endif  // CHROME_BROWSER_UI_STARTUP_FIRST_RUN_LACROS_H_
