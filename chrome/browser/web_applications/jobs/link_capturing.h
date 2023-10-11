// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_LINK_CAPTURING_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_LINK_CAPTURING_H_

#include "components/webapps/common/web_app_id.h"

namespace base {
class Value;
}  // namespace base

namespace web_app {
class AllAppsLock;

base::Value SetAppCapturesSupportedLinksDisableOverlapping(
    const webapps::AppId& app_id,
    bool set_to_preferred,
    AllAppsLock& lock);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_LINK_CAPTURING_H_
