// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_icon_waiter.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"

WebAppIconWaiter::WebAppIconWaiter(Profile* profile,
                                   const webapps::AppId& app_id)
    : app_id_(app_id) {
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForTest(profile);
  if (web_app_provider->icon_manager().GetFavicon(app_id).empty()) {
    web_app_provider->icon_manager().SetFaviconReadCallbackForTesting(
        base::BindRepeating(&WebAppIconWaiter::OnFaviconRead,
                            base::Unretained(this)));
  } else {
    run_loop_.Quit();
  }
}
void WebAppIconWaiter::Wait() {
  run_loop_.Run();
}
void WebAppIconWaiter::OnFaviconRead(const webapps::AppId& app_id) {
  if (app_id == *app_id_) {
    run_loop_.Quit();
  }
}
