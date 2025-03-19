// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_DELEGATE_H_

class GURL;

namespace base {

class FilePath;

}  // namespace base

namespace content {

struct PathInfo;

}  // namespace content

namespace web_app {

struct WebAppLaunchParams;

class LaunchQueueDelegate {
 public:
  virtual ~LaunchQueueDelegate() = default;

  virtual bool IsInScope(const WebAppLaunchParams& launch_params,
                         const GURL& current_url) const = 0;

  virtual content::PathInfo GetPathInfo(
      const base::FilePath& entry_path) const = 0;

  virtual bool IsValidLaunchParams(const WebAppLaunchParams& params) const = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_DELEGATE_H_
