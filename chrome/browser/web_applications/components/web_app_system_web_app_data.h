// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SYSTEM_WEB_APP_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SYSTEM_WEB_APP_DATA_H_

#include <iosfwd>
#include "base/optional.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

namespace web_app {

struct WebAppSystemWebAppData {
  SystemAppType system_app_type;
};

// For logging and debugging purposes.
std::ostream& operator<<(std::ostream& out, const WebAppSystemWebAppData& data);

bool operator==(const WebAppSystemWebAppData& data1,
                const WebAppSystemWebAppData& data2);
bool operator!=(const WebAppSystemWebAppData& data1,
                const WebAppSystemWebAppData& data2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_SYSTEM_WEB_APP_DATA_H_
