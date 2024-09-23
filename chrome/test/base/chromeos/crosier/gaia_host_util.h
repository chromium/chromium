// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_GAIA_HOST_UTIL_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_GAIA_HOST_UTIL_H_

#include "chrome/browser/ash/login/test/js_checker.h"

namespace content {
class RenderFrameHost;
}

namespace crosier {

// Get the `RenderFrameHost` backing Gaia webview.
content::RenderFrameHost* GetGaiaHost();

// Get a `JSCheck` instance to run javascript inside Gaia webview.
ash::test::JSChecker GaiaFrameJS();

// Skip to Gaia screen and wait for it to be ready.
void SkipToGaiaScreenAndWait();

}  // namespace crosier

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_GAIA_HOST_UTIL_H_
