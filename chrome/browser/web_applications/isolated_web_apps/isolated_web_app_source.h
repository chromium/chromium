// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_SOURCE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_SOURCE_H_

#include "base/files/file_path.h"
#include "url/origin.h"

namespace web_app {

// TODO(crbug.com/326044349): This is temporary and will be refactored.
struct IwaSourceBundle {
  bool operator==(const IwaSourceBundle& other) const;

  base::FilePath path;
};

// TODO(crbug.com/326044349): This is temporary and will be refactored.
struct IwaSourceProxy {
  bool operator==(const IwaSourceProxy& other) const;

  url::Origin proxy_url;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_SOURCE_H_
