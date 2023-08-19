// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the shared command-line switches used by code in the Chrome
// directory that don't have anywhere more specific to go.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_SWITCHES_H_
#define COMPONENTS_EMBEDDER_SUPPORT_SWITCHES_H_

// Switches used by multiple embedders.
namespace embedder_support {

extern const char kDisableAutoReload[];
extern const char kDisablePopupBlocking[];
extern const char kEnableAutoReload[];
extern const char kHeadless[];
extern const char kOriginTrialDisabledFeatures[];
extern const char kOriginTrialDisabledTokens[];
extern const char kOriginTrialPublicKey[];
extern const char kShortReportingDelay[];
extern const char kUserAgent[];

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_SWITCHES_H_
