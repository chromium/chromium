// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INITIALIZE_EXTENSIONS_CLIENT_H_
#define CHROME_COMMON_INITIALIZE_EXTENSIONS_CLIENT_H_

#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#error "Extensions must be enabled"
#endif

// Initializes the single instance of the ExtensionsClient. Safe to call
// multiple times.
void EnsureExtensionsClientInitialized();

#endif  // CHROME_COMMON_INITIALIZE_EXTENSIONS_CLIENT_H_
