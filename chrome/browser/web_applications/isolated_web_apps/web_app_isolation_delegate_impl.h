// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WEB_APP_ISOLATION_DELEGATE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WEB_APP_ISOLATION_DELEGATE_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/web_app_isolation_delegate.h"

class Profile;

namespace web_app {

class WebAppIsolationDelegateImpl : public WebAppIsolationDelegate {
 public:
  static std::unique_ptr<WebAppIsolationDelegate> Create(Profile* profile);

  ~WebAppIsolationDelegateImpl() override;

  // WebAppIsolationDelegate:
  void ClearAppResourcesOnUninstall(const webapps::AppId& app_id,
                                    base::OnceClosure callback) override;

 private:
  explicit WebAppIsolationDelegateImpl(Profile* profile);

  const raw_ref<Profile> profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_WEB_APP_ISOLATION_DELEGATE_IMPL_H_
