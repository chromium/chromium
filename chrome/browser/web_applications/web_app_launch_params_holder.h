// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_PARAMS_HOLDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_PARAMS_HOLDER_H_

#include "base/memory/weak_ptr.h"
#include "components/webapps/common/web_app_id.h"

namespace webapps {
class LaunchParams;
}

namespace web_app {

// Pure virtual backend interface for classes that hold and manage the active
// parameters of an in-progress web app launch navigation.
//
// This interface decouples backend core objects (like WebAppTabHelper) from
// UI-layer classes (like WebAppLaunchNavigationHandleUserData), allowing them
// to safely track the active navigations without violating layer
// dependencies.
class WebAppLaunchParamsHolder {
 public:
  virtual ~WebAppLaunchParamsHolder() = default;
  virtual const webapps::AppId& app_id() const = 0;
  virtual const webapps::LaunchParams& GetLaunchParams() const = 0;
  virtual void SetLaunchParams(webapps::LaunchParams launch_params) = 0;

  virtual base::WeakPtr<WebAppLaunchParamsHolder> GetWeakPtr() = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_PARAMS_HOLDER_H_
