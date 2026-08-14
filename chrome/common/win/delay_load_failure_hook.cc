// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

#include "chrome/common/win/delay_load_failure_support.h"

// Set the delay load failure hook to the common failure handler so that crash
// reports are generated rather than structured exceptions being thrown.
//
// The |__pfnDliFailureHook2| failure notification hook gets called
// automatically by the delay load runtime in case of failure, see
// https://docs.microsoft.com/en-us/cpp/build/reference/failure-hooks?view=vs-2019
// for more information about this.
extern "C" const PfnDliHook __pfnDliFailureHook2 = HandleDelayLoadFailureCommon;
