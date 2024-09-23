// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include <map>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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
  WebAppFileHandlerRegistrationLinuxBrowserTest() = default;

  Profile* profile() { return browser()->profile(); }

  void InstallApp(ExternalInstallOptions install_options) {
    auto result = ExternallyManagedAppManagerInstall(browser()->profile(),
                                                     install_options);
    result_code_ = result.code;
  }

  void TearDownOnMainThread() override {
  }

  const base::FilePath GetUserApplicationsDir() {
    return override_registration_.test_override().applications();
  }

  std::optional<webapps::InstallResultCode> result_code_;
  OsIntegrationTestOverrideBlockingRegistration override_registration_;
};

// Verify that the MIME type registration callback is called with
// the correct xdg-utility commands and data.
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

  std::vector<LinuxFileRegistration> xdg_commands_called;
  base::RunLoop loop;
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(base::BindLambdaForTesting(
      [&](base::FilePath filename_in, std::string xdg_command_in,
          std::string file_contents_in) {
        LinuxFileRegistration file_registration = LinuxFileRegistration();
        file_registration.file_name = filename_in;
        file_registration.xdg_command = xdg_command_in;
        file_registration.file_contents = file_contents_in;
        xdg_commands_called.push_back(file_registration);
        loop.Quit();
        return true;
      }));

  // Override the source as default apps don't get file handlers registered.
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.install_source = ExternalInstallSource::kExternalPolicy;
  InstallApp(install_options);

  loop.Run();
  std::optional<webapps::AppId> app_id = WebAppProvider::GetForTest(profile())
                                             ->registrar_unsafe()
                                             .LookupExternalAppId(url);
  EXPECT_TRUE(app_id.has_value());

  base::FilePath expected_filename =
      shell_integration_linux::GetMimeTypesRegistrationFilename(
          profile()->GetPath(), app_id.value());

  // The first call is the xdg-mime one, which contains the XML for the file
  // handlers.
  EXPECT_EQ(xdg_commands_called[0].file_contents, expected_file_contents);
  EXPECT_EQ(xdg_commands_called[0].file_name, expected_filename);
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(
      UpdateMimeInfoDatabaseOnLinuxCallback());
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());

  // The second call should be the one that refreshes the mimeinfo.cache, i.e.
  // the update-desktop-database call.
  EXPECT_TRUE(base::StartsWith(xdg_commands_called[1].xdg_command,
                               "update-desktop-database"));
  EXPECT_TRUE(base::Contains(xdg_commands_called[1].xdg_command,
                             GetUserApplicationsDir().value()));
}

}  // namespace web_app
