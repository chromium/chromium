// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_DELEGATE_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/webapps/browser/launch_queue/launch_queue_delegate.h"

class GURL;

namespace base {

class FilePath;

}  // namespace base

namespace content {

struct PathInfo;

}  // namespace content

namespace webapps {

struct LaunchParams;

}

namespace web_app {

class WebAppRegistrar;

class LaunchQueueDelegateImpl final : public webapps::LaunchQueueDelegate {
 public:
  explicit LaunchQueueDelegateImpl(const WebAppRegistrar& registrar);

  LaunchQueueDelegateImpl(const LaunchQueueDelegateImpl&) = delete;
  LaunchQueueDelegateImpl& operator=(const LaunchQueueDelegateImpl&) = delete;

  ~LaunchQueueDelegateImpl() override = default;

  bool IsInScope(const webapps::LaunchParams& launch_params,
                 const GURL& current_url) const override;

  content::PathInfo GetPathInfo(
      const base::FilePath& entry_path) const override;

  bool IsValidLaunchParams(const webapps::LaunchParams& params) const override;

 private:
  const raw_ref<const WebAppRegistrar> registrar_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_DELEGATE_IMPL_H_
