// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include <map>
#include <string>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
    : public InProcessBrowserTest {
 protected:
  WebAppFileHandlerRegistrationLinuxBrowserTest() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    override_registration_ = OsIntegrationTestOverride::OverrideForTesting();
  }

  WebAppRegistrar& registrar() {
    return WebAppProvider::GetForTest(browser()->profile())->registrar_unsafe();
  }

  void InstallApp(ExternalInstallOptions install_options) {
    auto result = ExternallyManagedAppManagerInstall(browser()->profile(),
                                                     install_options);
    result_code_ = result.code;
  }

  void TearDownOnMainThread() override {
    test::UninstallAllWebApps(browser()->profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      override_registration_.reset();
    }
  }

  absl::optional<webapps::InstallResultCode> result_code_;
  std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
      override_registration_;
};

// Verify that the MIME type registration callback is called and that
// the caller behaves as expected.
IN_PROC_BROWSER_TEST_F(
    WebAppFileHandlerRegistrationLinuxBrowserTest,
    UpdateMimeInfoDatabaseOnLinuxCallbackCalledSuccessfully) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_file_handlers.json"));

  apps::FileHandlers expected_file_handlers;
  expected_file_handlers.push_back(GetTestFileHandler(
      "https://site.api/open-foo",
      {{"application/foo", {".foo"}}, {"application/foobar", {".foobar"}}}));
  expected_file_handlers.push_back(GetTestFileHandler(
      "https://site.api/open-bar", {{"application/bar", {".bar", ".baz"}}}));

  std::string expected_file_contents =
      shell_integration_linux::GetMimeTypesRegistrationFileContents(
          expected_file_handlers);

  base::FilePath filename;
  std::string xdg_command;
  std::string file_contents;
  base::RunLoop loop;
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(base::BindLambdaForTesting(
      [&](base::FilePath filename_in, std::string xdg_command_in,
          std::string file_contents_in) {
        filename = filename_in;
        xdg_command = xdg_command_in;
        file_contents = file_contents_in;
        loop.Quit();
        return true;
      }));

  // Override the source as default apps don't get file handlers registered.
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.install_source = ExternalInstallSource::kExternalPolicy;
  InstallApp(install_options);

  loop.Run();
  EXPECT_EQ(file_contents, expected_file_contents);
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(
      UpdateMimeInfoDatabaseOnLinuxCallback());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
}

}  // namespace web_app
