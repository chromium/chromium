// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_

#include <stdint.h>
#include <memory>

#include "base/strings/string_piece_forward.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "content/public/browser/service_worker_context.h"
#include "url/gurl.h"

class Browser;
class Profile;
struct WebAppInstallInfo;

namespace content {
class StoragePartition;
class WebContents;
}  // namespace content

namespace web_app {

class WebApp;

namespace test {

std::unique_ptr<WebApp> CreateWebApp(
    const GURL& start_url = GURL("https://example.com/path"),
    WebAppManagement::Type source_type = WebAppManagement::kSync);

std::unique_ptr<WebApp> CreateRandomWebApp(const GURL& base_url, uint32_t seed);

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback);

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback);

AppId InstallPwaForCurrentUrl(Browser* browser);

void CheckServiceWorkerStatus(const GURL& url,
                              content::StoragePartition* storage_partition,
                              content::ServiceWorkerCapability status);

void SetWebAppSettingsListPref(Profile* profile, base::StringPiece pref);

}  // namespace test
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_
