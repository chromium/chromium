// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include <map>
#include <string>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
    : public InProcessBrowserTest {
 protected:
  WebAppFileHandlerRegistrationLinuxBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kFileHandlingAPI);
  }

  WebAppRegistrar& registrar() {
    return WebAppProvider::GetForTest(browser()->profile())->registrar_unsafe();
  }

  void InstallApp(ExternalInstallOptions install_options) {
    auto result = ExternallyManagedAppManagerInstall(browser()->profile(),
                                                     install_options);
    result_code_ = result.code;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  absl::optional<webapps::InstallResultCode> result_code_;
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
  UpdateMimeInfoDatabaseOnLinuxCallback callback = base::BindLambdaForTesting(
      [&expected_file_contents, &path_reached, &run_loop](
          base::FilePath filename, std::string xdg_command,
          std::string file_contents) {
        EXPECT_EQ(file_contents, expected_file_contents);
        path_reached = true;
        run_loop.Quit();
        return true;
      });
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(std::move(callback));

  // Override the source as default apps don't get file handlers registered.
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.install_source = ExternalInstallSource::kExternalPolicy;
  InstallApp(install_options);
  run_loop.Run();
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  ASSERT_TRUE(path_reached);
}

}  // namespace web_app
