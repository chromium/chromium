// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_

#include <stdint.h>

#include <memory>
#include <optional>  // for optional, nullopt
#include <string_view>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

class Browser;
class PrefService;
class Profile;

namespace content {
class StoragePartition;
class WebContents;
enum class ServiceWorkerCapability;
}  // namespace content

namespace web_app {

class WebApp;
class WebAppSyncBridge;
struct WebAppInstallInfo;

namespace test {

// Do not use this for installation! Instead, use the utilities in
// web_app_install_test_util.h.
std::unique_ptr<WebApp> CreateWebApp(
    const GURL& start_url = GURL("https://example.com/path"),
    WebAppManagement::Type source_type = WebAppManagement::kSync);

// Do not use this for installation! Instead, use the utilities in
// web_app_install_test_util.h.
struct CreateRandomWebAppParams {
  GURL base_url{"https://example.com/path"};
  uint32_t seed = 0;
  bool non_zero = false;
  bool allow_system_source = true;
};
std::unique_ptr<WebApp> CreateRandomWebApp(CreateRandomWebAppParams params);

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback);

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback);

webapps::AppId InstallPwaForCurrentUrl(Browser* browser);

void CheckServiceWorkerStatus(const GURL& url,
                              content::StoragePartition* storage_partition,
                              content::ServiceWorkerCapability status);

void SetWebAppSettingsListPref(Profile* profile, std::string_view pref);

void AddInstallUrlData(PrefService* pref_service,
                       WebAppSyncBridge* sync_bridge,
                       const webapps::AppId& app_id,
                       const GURL& url,
                       const ExternalInstallSource& source);

void AddInstallUrlAndPlaceholderData(PrefService* pref_service,
                                     WebAppSyncBridge* sync_bridge,
                                     const webapps::AppId& app_id,
                                     const GURL& url,
                                     const ExternalInstallSource& source,
                                     bool is_placeholder);

void SynchronizeOsIntegration(
    Profile* profile,
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> options = std::nullopt);

// Creates a few well-formed integrity block signatures.
std::vector<web_package::SignedWebBundleSignatureInfo> CreateSignatures();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class ScopedSkipMainProfileCheck {
 public:
  ScopedSkipMainProfileCheck();
  ScopedSkipMainProfileCheck(const ScopedSkipMainProfileCheck&) = delete;
  ScopedSkipMainProfileCheck& operator=(const ScopedSkipMainProfileCheck&) =
      delete;
  ~ScopedSkipMainProfileCheck();
};
#endif

}  // namespace test
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_UTILS_H_
