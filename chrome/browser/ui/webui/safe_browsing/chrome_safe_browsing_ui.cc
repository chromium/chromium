// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/safe_browsing/chrome_safe_browsing_ui.h"

#include "chrome/browser/safe_browsing/chrome_safe_browsing_local_state_delegate.h"

namespace safe_browsing {

ChromeSafeBrowsingUI::ChromeSafeBrowsingUI(content::WebUI* web_ui)
    : SafeBrowsingUI(
          web_ui,
          std::make_unique<ChromeSafeBrowsingLocalStateDelegate>(web_ui)) {}

ChromeSafeBrowsingUI::~ChromeSafeBrowsingUI() = default;

}  // namespace safe_browsing
