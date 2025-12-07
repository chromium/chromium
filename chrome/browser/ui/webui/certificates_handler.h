// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATES_HANDLER_H_

#include "build/build_config.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace certificate_manager {
  // Register profile preferences.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
}  // namespace certificate_manager

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATES_HANDLER_H_
