// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_

#include <string>

namespace content {
class BrowserContext;
}

namespace web_app {

// Creates and registers a URLDataSource that serves a blank page with
// a manifest.
void AddTestURLDataSource(const std::string& source_name,
                          content::BrowserContext* browser_context);
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_TEST_TEST_SYSTEM_WEB_APP_URL_DATA_SOURCE_H_
