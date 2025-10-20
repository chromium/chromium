// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_command.h"

#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {
namespace {
static constexpr std::string_view kInstallUrl =
    "https://example.com/install.html";
static constexpr std::string_view kStartUrl =
    "https://example.com/path/app.html";
static constexpr std::string_view kManifestUrl =
    "https://www.otherorigin.com/path/manifest.json";

class FetchManifestAndUpdateTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  blink::mojom::ManifestPtr& GetPageManifest() {
    return web_contents_manager()
        .GetOrCreatePageState(GURL(kInstallUrl))
        .manifest_before_default_processing;
  }

  std::optional<webapps::AppId> InstallApp() {
    const webapps::AppId app_id =
        web_contents_manager().CreateBasicInstallPageState(
            GURL(kInstallUrl), GURL(kManifestUrl), GURL(kStartUrl));

    web_contents_manager().SetUrlLoaded(web_contents(), GURL(kInstallUrl));

    const webapps::AppId installed_app_id = test::InstallForWebContents(
        profile(), web_contents(),
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
    return app_id == installed_app_id ? std::optional<webapps::AppId>(app_id)
                                      : std::nullopt;
  }
};

TEST_F(FetchManifestAndUpdateTest, NameUpdate) {
  ASSERT_OK_AND_ASSIGN(webapps::AppId app_id, InstallApp());

  GetPageManifest()->name = u"New Name";

  base::test::TestFuture<FetchManifestAndUpdateResult> future;
  provider().scheduler().FetchManifestAndUpdate(
      GURL(kInstallUrl), GenerateManifestIdFromStartUrlOnly(GURL(kStartUrl)),
      future.GetCallback());
  ASSERT_EQ(future.Get(), FetchManifestAndUpdateResult::kSuccess);
  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(app_id), "New Name");
}

// TODO(http://crbug.com/452416687): Add tests for other updatable items, and
// make sure the trusted icons update.

// TODO(http://crbug.com/452416687): Add tests for failure conditions:
// - Url load failure
// - Primary page change
// - Icon load failure
// - Update failure

}  // namespace
}  // namespace web_app
