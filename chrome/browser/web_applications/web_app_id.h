// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_H_

#include <string>

namespace web_app {

// App ID matches Extension ID.
using AppId = std::string;

// Unhashed version of App ID. This can be hashed using
// GenerateAppIdFromUnhashed(unhashed_app_id), see
// chrome/browser/web_applications/web_app_helpers.h.
using UnhashedAppId = std::string;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_H_
