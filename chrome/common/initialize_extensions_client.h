// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INITIALIZE_EXTENSIONS_CLIENT_H_
#define CHROME_COMMON_INITIALIZE_EXTENSIONS_CLIENT_H_

#include "base/containers/span.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/features/feature.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

#include "extensions/common/features/feature.h"

base::span<const char* const> GetControlledFrameFeatureList();

// Initializes the single instance of the ExtensionsClient. Safe to call
// multiple times.
void EnsureExtensionsClientInitialized(
    extensions::Feature::FeatureDelegatedAvailabilityCheckMap);

void EnsureExtensionsClientInitialized();

#endif  // CHROME_COMMON_INITIALIZE_EXTENSIONS_CLIENT_H_
