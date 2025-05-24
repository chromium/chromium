// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {

class ManifestUpdateCheckCommandV2Test : public WebAppTest {
 public:
  ManifestUpdateCheckCommandV2Test() = default;
  ManifestUpdateCheckCommandV2Test(const ManifestUpdateCheckCommandV2Test&) =
      delete;
  ManifestUpdateCheckCommandV2Test& operator=(
      const ManifestUpdateCheckCommandV2Test&) = delete;
  ~ManifestUpdateCheckCommandV2Test() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    web_contents_manager().SetUrlLoaded(web_contents(), app_url());
  }

 protected:
  struct RunResult {
    ManifestUpdateCheckResult check_result;
    std::unique_ptr<WebAppInstallInfo> new_install_info;
  };

  void SetupPageState() {
    auto& page_state = web_contents_manager().GetOrCreatePageState(app_url());

    page_state.manifest_url = GURL("https://www.example.com/manifest.json");
    page_state.has_service_worker = false;
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;

    // Set up manifest icon.
    blink::Manifest::ImageResource icon;
    icon.src = default_icon_url_;
    icon.sizes = {{manifest_icon_size_, manifest_icon_size_}};
    icon.purpose = {blink::mojom::ManifestImageResource_Purpose::ANY};

    // Set icons in content.
    web_contents_manager().GetOrCreateIconState(default_icon_url_).bitmaps = {
        gfx::test::CreateBitmap(manifest_icon_size_, manifest_icon_color_)};

    // Set up manifest.
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = app_url();
    manifest->id = GenerateManifestIdFromStartUrlOnly(app_url());
    manifest->scope = app_url().GetWithoutFilename();
    manifest->display = DisplayMode::kStandalone;
    manifest->name = u"Foo App";
    manifest->icons = {icon};

    page_state.manifest_before_default_processing = std::move(manifest);
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  GURL app_url() { return app_url_; }

 private:
  const GURL app_url_{"http://www.foo.bar/web_apps/basic.html"};
  const GURL default_icon_url_{"https://example.com/path/def_icon.png"};
  const SkColor manifest_icon_color_ = SK_ColorCYAN;
  const int manifest_icon_size_ = 96;
};

TEST_F(ManifestUpdateCheckCommandV2Test, Verify) {
  SetupPageState();
  webapps::AppId app_id = test::InstallForWebContents(
      profile(), web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  // Running command.
  base::test::TestFuture<ManifestUpdateCheckResult,
                         std::unique_ptr<WebAppInstallInfo>>
      manifest_update_check_future;
  provider().scheduler().ScheduleManifestUpdateCheckV2(
      app_url(), app_id, base::Time::Now(), web_contents()->GetWeakPtr(),
      manifest_update_check_future.GetCallback());

  EXPECT_TRUE(manifest_update_check_future.Wait());
}

}  // namespace web_app
