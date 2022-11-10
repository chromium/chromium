// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_WAITER_H_

#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"

class Profile;

class WebAppIconWaiter {
 public:
  explicit WebAppIconWaiter(Profile* profile, const web_app::AppId& app_id);

  void Wait();

 private:
  void OnFaviconRead(const web_app::AppId& app_id);
  const raw_ref<const web_app::AppId> app_id_;
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_ICON_WAITER_H_
