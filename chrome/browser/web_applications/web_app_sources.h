// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SOURCES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SOURCES_H_

#include <bitset>

#include "chrome/browser/web_applications/web_app_constants.h"

namespace web_app {

using WebAppSources = std::bitset<WebAppManagement::kMaxValue + 1>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SOURCES_H_
