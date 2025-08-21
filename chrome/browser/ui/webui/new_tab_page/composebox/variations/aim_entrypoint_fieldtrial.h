// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_AIM_ENTRYPOINT_FIELDTRIAL_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_AIM_ENTRYPOINT_FIELDTRIAL_H_

#include "base/feature_list.h"

class Profile;

namespace ntp_composebox {

// If overridden to false, disables the feature (kill switch). If true, enables
// the feature beyond English in the US (subject to eligibility checks).
BASE_DECLARE_FEATURE(kNtpSearchboxComposeEntrypoint);

// Enables or disables the NTP searchbox entrypoint for English in the US.
BASE_DECLARE_FEATURE(kNtpSearchboxComposeEntrypointEnglishUs);

bool IsNtpSearchboxComposeEntrypointEnabled(Profile* profile);

}  // namespace ntp_composebox

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_AIM_ENTRYPOINT_FIELDTRIAL_H_
