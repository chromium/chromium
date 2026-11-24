// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bidi_mapper/host/features.h"

namespace bidi_mapper::features {

// Enables the chrome.driver extension API, which lets extensions automate the
// whole browser over WebDriver BiDi. This is an unsafe feature: the browser
// shows a security warning at startup when it is enabled.
BASE_FEATURE(kChromeDriverExtensionApi, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace bidi_mapper::features
