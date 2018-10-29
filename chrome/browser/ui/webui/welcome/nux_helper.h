// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_HELPER_H_

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"

class Profile;

namespace nux {
bool IsNuxOnboardingEnabled(Profile* profile);
}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_HELPER_H_
