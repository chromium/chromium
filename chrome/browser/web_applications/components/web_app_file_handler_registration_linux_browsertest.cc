// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include <map>
#include <string>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using AcceptMap = std::map<std::string, base::flat_set<std::string>>;

apps::FileHandler GetTestFileHandler(const std::string& action,
                                     const AcceptMap& accept_map) {
  apps::FileHandler file_handler;
  file_handler.action = GURL(action);
  for (const auto& elem : accept_map) {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = elem.first;
    accept_entry.file_extensions.insert(elem.second.begin(), elem.second.end());
    file_handler.accept.push_back(accept_entry);
  }
  return file_handler;
}

}  // namespace

class WebAppFileHandlerRegistrationLinuxBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<ProviderType> {
 protected:
  WebAppFileHandlerRegistrationLinuxBrowserTest() {
    if (GetParam() == ProviderType::kWebApps) {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kNativeFileSystemAPI,
           blink::features::kFileHandlingAPI,
           features::kDesktopPWAsWithoutExtensions},
          {});
    } else if (GetParam() == ProviderType::kBookmarkApps) {
      scoped_feature_list_.InitWithFeatures(
          {blink::features::kNativeFileSystemAPI,
           blink::features::kFileHandlingAPI},
          {features::kDesktopPWAsWithoutExtensions});
    }
  }

  AppRegistrar& registrar() {
    return WebAppProviderBase::GetProviderBase(browser()->profile())
        ->registrar();
  }

  void InstallApp(ExternalInstallOptions install_options) {
    result_code_ = web_app::PendingAppManagerInstall(browser()->profile(),
                                                     install_options);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::Optional<InstallResultCode> result_code_;
};

// Verify that the MIME type registration callback is called and that
// the caller behaves as expected.
IN_PROC_BROWSER_TEST_P(WebAppFileHandlerRegistrationLinuxBrowserTest,
                       RegisterMimeTypesOnLinuxCallbackCalledSuccessfully) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json"));

  apps::FileHandlers file_handlers;
  file_handlers.push_back(GetTestFileHandler(
      "https://site.api/open-foo",
      {{"application/foo", {".foo"}}, {"application/foobar", {".foobar"}}}));
  file_handlers.push_back(GetTestFileHandler(
      "https://site.api/open-bar", {{"application/bar", {".bar", ".baz"}}}));

  std::string expected_file_contents =
      shell_integration_linux::GetMimeTypesRegistrationFileContents(
          file_handlers);
  bool path_reached = false;
  base::RunLoop run_loop;
  RegisterMimeTypesOnLinuxCallback callback = base::BindLambdaForTesting(
      [&expected_file_contents, &path_reached, &run_loop](
          base::FilePath filename, std::string file_contents) {
        EXPECT_EQ(file_contents, expected_file_contents);
        path_reached = true;
        run_loop.Quit();
        return true;
      });
  SetRegisterMimeTypesOnLinuxCallbackForTesting(std::move(callback));

  InstallApp(CreateInstallOptions(url));
  run_loop.Run();
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  ASSERT_TRUE(path_reached);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppFileHandlerRegistrationLinuxBrowserTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         ProviderTypeParamToString);

}  // namespace web_app
