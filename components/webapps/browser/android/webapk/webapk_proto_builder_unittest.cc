// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_proto_builder.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapp_icon.h"
#include "components/webapps/browser/features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

const base::FilePath::CharType kTestDataDir[] =
    FILE_PATH_LITERAL("components/test/data/webapps");

// The URLs of best icons from Web Manifest. We use a random file in the test
// data directory. Since WebApkInstaller does not try to decode the file as an
// image it is OK that the file is not an image.
const char* kBestPrimaryIconUrl = "/simple.html";
const char* kBestSplashIconUrl = "/nostore.html";
const char* kBestShortcutIconUrl = "/title1.html";

const char* kUnusedIconPath = "https://example.com/unused_icon.png";

std::unique_ptr<webapps::WebappIcon> BuildTestWebApkIcon(
    GURL icon_url,
    std::string data,
    std::string hash,
    std::set<webapk::Image::Usage> usages) {
  auto icon = std::make_unique<webapps::WebappIcon>(icon_url);
  icon->SetData(std::move(data));
  icon->set_hash(std::move(hash));
  for (const auto& usage : usages) {
    icon->AddUsage(usage);
  }
  return icon;
}

}  // namespace

// Builds WebApk proto and blocks till done.
class BuildProtoRunner {
 public:
  BuildProtoRunner() = default;

  BuildProtoRunner(const BuildProtoRunner&) = delete;
  BuildProtoRunner& operator=(const BuildProtoRunner&) = delete;

  ~BuildProtoRunner() = default;

  void BuildSync(
      const GURL& best_primary_icon_url,
      const GURL& splash_image_url,
      std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons,
      std::unique_ptr<webapps::WebappIcon> primary_icon,
      std::unique_ptr<webapps::WebappIcon> splash_icon,
      const GURL& manifest_id,
      const GURL& app_key,
      const std::optional<SkColor>& dark_theme_color,
      const std::optional<SkColor>& dark_background_color,
      bool is_manifest_stale,
      bool is_app_identity_update_supported,
      const std::vector<GURL>& best_shortcut_icon_urls) {
    webapps::ShortcutInfo info{GURL()};
    info.manifest_id = manifest_id;
    info.dark_theme_color = dark_theme_color;
    info.dark_background_color = dark_background_color;
    info.best_primary_icon_url = best_primary_icon_url;
    info.splash_image_url = splash_image_url;
    if (!best_primary_icon_url.is_empty())
      info.icon_urls.push_back(best_primary_icon_url.spec());
    if (!splash_image_url.is_empty())
      info.icon_urls.push_back(splash_image_url.spec());
    info.icon_urls.push_back(kUnusedIconPath);

    for (const GURL& shortcut_url : best_shortcut_icon_urls) {
      info.best_shortcut_icon_urls.push_back(shortcut_url);
      info.shortcut_items.emplace_back();
      info.shortcut_items.back().icons.emplace_back();
      info.shortcut_items.back().icons.back().src = shortcut_url;
    }

    webapps::BuildProto(info, app_key, std::move(primary_icon),
                        std::move(splash_icon), "" /* package_name */,
                        "" /* version */, std::move(webapk_icons),
                        is_manifest_stale, is_app_identity_update_supported,
                        base::BindOnce(&BuildProtoRunner::OnBuiltWebApkProto,
                                       base::Unretained(this)));

    run_loop_.Run();
  }

  // Helper function to build proto without |primary_icon|, similar to install
  // new WebAPKs. All icon information should come from |webapk_icons|
  void BuildForIconInstallTestSync(
      const GURL& icon_url,
      std::vector<std::string> icon_urls,
      std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons) {
    webapps::ShortcutInfo info{GURL()};
    info.best_primary_icon_url = icon_url;
    info.icon_urls = std::move(icon_urls);

    webapps::BuildProto(info, GURL() /* app_key */, nullptr /* primary_icon */,
                        nullptr /* splash_icon */, "" /* package_name */,
                        "" /* version */, std::move(webapk_icons),
                        false /* is_manifest_stale */,
                        false /* is_app_identity_update_supported */,
                        base::BindOnce(&BuildProtoRunner::OnBuiltWebApkProto,
                                       base::Unretained(this)));

    run_loop_.Run();
  }

  // Helper function to build proto with |primary_icon|, similar to update
  // existing WebAPKs.
  void BuildForIconUpdateTestSync(
      const GURL& icon_url,
      std::vector<std::string> icon_urls,
      std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons,
      const std::string& icon_data,
      const std::string& icon_hash) {
    webapps::ShortcutInfo info{GURL()};
    info.best_primary_icon_url = icon_url;
    info.icon_urls = std::move(icon_urls);

    auto primary_icon = BuildTestWebApkIcon(icon_url, icon_data, icon_hash,
                                            {webapk::Image::PRIMARY_ICON});

    webapps::BuildProto(info, GURL() /* app_key */, std::move(primary_icon),
                        nullptr /* splash_icon */, "" /* package_name */,
                        "" /* version */, std::move(webapk_icons),
                        false /* is_manifest_stale */,
                        false /* is_app_identity_update_supported */,
                        base::BindOnce(&BuildProtoRunner::OnBuiltWebApkProto,
                                       base::Unretained(this)));
    run_loop_.Run();
  }

  webapk::WebApk* GetWebApkRequest() { return webapk_request_.get(); }

 private:
  // Called when the |webapk_request_| is populated.
  void OnBuiltWebApkProto(std::unique_ptr<std::string> serialized_proto) {
    webapk_request_ = std::make_unique<webapk::WebApk>();
    webapk_request_->ParseFromString(*serialized_proto);
    run_loop_.QuitClosure().Run();
  }

  // The populated webapk::WebApk.
  std::unique_ptr<webapk::WebApk> webapk_request_;

  // Called after the |webapk_request_| is built.
  base::RunLoop run_loop_;
};

class WebApkProtoBuilderTest : public ::testing::Test {
 public:
  std::unique_ptr<BuildProtoRunner> CreateBuildProtoRunner() {
    return std::make_unique<BuildProtoRunner>();
  }

  WebApkProtoBuilderTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  WebApkProtoBuilderTest(const WebApkProtoBuilderTest&) = delete;
  WebApkProtoBuilderTest& operator=(const WebApkProtoBuilderTest&) = delete;

  ~WebApkProtoBuilderTest() override = default;

  void SetUp() override {
    test_server_.AddDefaultHandlers(base::FilePath(kTestDataDir));
    ASSERT_TRUE(test_server_.Start());
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  net::EmbeddedTestServer test_server_;

  content::BrowserTaskEnvironment task_environment_;
};

// When there is no Web Manifest available for a site, an empty
// |best_primary_icon_url| and an empty |splash_image_url| is used to build a
// WebApk update request. Tests the request can be built properly.
TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoWhenManifestIsObsolete) {
  GURL icon_url_1 = test_server()->GetURL("/icon1.png");
  GURL icon_url_2 = test_server()->GetURL("/icon2.png");
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(icon_url_1,
                       BuildTestWebApkIcon(icon_url_1, "data1", "1",
                                           {webapk::Image::PRIMARY_ICON}));
  webapk_icons.emplace(icon_url_2,
                       BuildTestWebApkIcon(icon_url_2, "data2", "2",
                                           {webapk::Image::SPLASH_ICON}));

  auto primary_icon = BuildTestWebApkIcon(GURL(), "data3", std::string(),
                                          {webapk::Image::PRIMARY_ICON});
  auto splash_icon = BuildTestWebApkIcon(GURL(), "data4", std::string(),
                                         {webapk::Image::SPLASH_ICON});

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), std::move(webapk_icons), std::move(primary_icon),
      std::move(splash_icon), GURL() /*manifest_id*/, GURL() /*app_key*/,
      0x000000 /*dark_theme_color*/, 0x000000 /*dark_background_color*/,
      true /* is_manifest_stale */, true /* is_app_identity_update_supported */,
      {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  EXPECT_EQ("", manifest.icons(0).src());
  EXPECT_FALSE(manifest.icons(0).has_hash());
  EXPECT_EQ(manifest.icons(0).image_data(), "data3");

  EXPECT_EQ("", manifest.icons(1).src());
  EXPECT_FALSE(manifest.icons(1).has_hash());
  EXPECT_EQ(manifest.icons(1).image_data(), "data4");

  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest.
TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoWhenManifestIsAvailable) {
  GURL icon_url_1 = test_server()->GetURL("/icon.png");
  GURL best_primary_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  GURL best_splash_icon_url = test_server()->GetURL(kBestSplashIconUrl);
  GURL best_shortcut_icon_url = test_server()->GetURL(kBestShortcutIconUrl);

  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(icon_url_1,
                       BuildTestWebApkIcon(icon_url_1, "data0", "0", {}));
  webapk_icons.emplace(best_primary_icon_url,
                       BuildTestWebApkIcon(best_primary_icon_url, "data1", "1",
                                           {webapk::Image::PRIMARY_ICON}));
  webapk_icons.emplace(best_splash_icon_url,
                       BuildTestWebApkIcon(best_splash_icon_url, "data2", "2",
                                           {webapk::Image::SPLASH_ICON}));
  webapk_icons.emplace(best_shortcut_icon_url,
                       BuildTestWebApkIcon(best_shortcut_icon_url, "data3", "3",
                                           {webapk::Image::SHORTCUT_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      best_primary_icon_url, best_splash_icon_url, std::move(webapk_icons),
      nullptr /* primary_icon */, nullptr /*splash_icon*/,
      GURL() /*manifest_id*/, GURL() /*app_key*/, 0x000000 /*dark_theme_color*/,
      0x000000 /*dark_background_color*/, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */, {best_shortcut_icon_url});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  // Check protobuf fields for kBestPrimaryIconUrl.
  EXPECT_EQ(best_primary_icon_url.spec(), manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(), "1");
  EXPECT_EQ(manifest.icons(0).image_data(), "data1");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  // Check protobuf fields for kBestSplashIconUrl.
  EXPECT_EQ(best_splash_icon_url.spec(), manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(), "2");
  EXPECT_EQ(manifest.icons(1).image_data(), "data2");
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(),
            best_shortcut_icon_url.spec());
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(), "3");
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(), "data3");
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest, and
// primary icon and splash icon share the same URL.
TEST_F(WebApkProtoBuilderTest,
       BuildWebApkProtoPrimaryIconAndSplashIconSameUrl) {
  GURL icon_url_1 = test_server()->GetURL("/icon.png");
  GURL best_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(icon_url_1,
                       BuildTestWebApkIcon(icon_url_1, "data1", "1", {}));
  webapk_icons.emplace(best_icon_url,
                       BuildTestWebApkIcon(best_icon_url, "data0", "0",
                                           {webapk::Image::PRIMARY_ICON,
                                            webapk::Image::SPLASH_ICON,
                                            webapk::Image::SHORTCUT_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(best_icon_url), GURL(best_icon_url), std::move(webapk_icons),
      nullptr /* primary_icon */, nullptr /*splash_icon*/,
      GURL() /*manifest_id*/, GURL() /*app_key*/, 0x000000 /*dark_theme_color*/,
      0x000000 /*dark_background_color*/, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */, {GURL(best_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(2, manifest.icons_size());

  // Check protobuf fields for icons.
  EXPECT_EQ(best_icon_url, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(), "0");
  EXPECT_EQ(manifest.icons(0).image_data(), "data0");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::UnorderedElementsAre(webapk::Image::PRIMARY_ICON,
                                            webapk::Image::SPLASH_ICON,
                                            webapk::Image::SHORTCUT_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(1).src());
  EXPECT_FALSE(manifest.icons(1).has_hash());
  EXPECT_FALSE(manifest.icons(1).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(), "0");
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(), "data0");
}

TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoWhenWithMultipleShortcuts) {
  GURL best_shortcut_icon_url1 = test_server()->GetURL(kBestShortcutIconUrl);
  GURL best_shortcut_icon_url2 = test_server()->GetURL(kBestPrimaryIconUrl);
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(
      best_shortcut_icon_url1,
      BuildTestWebApkIcon(best_shortcut_icon_url1, "data1", "1",
                          {webapk::Image::SHORTCUT_ICON}));
  webapk_icons.emplace(
      best_shortcut_icon_url2,
      BuildTestWebApkIcon(best_shortcut_icon_url2, "data2", "2",
                          {webapk::Image::SHORTCUT_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), std::move(webapk_icons), nullptr /* primary_icon */,
      nullptr /*splash_icon*/, GURL() /*manifest_id*/, GURL() /*app_key*/,
      0x000000 /*dark_theme_color*/, 0x000000 /*dark_background_color*/,
      false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */,
      {GURL(best_shortcut_icon_url1), GURL(best_shortcut_icon_url2)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(manifest.shortcuts_size(), 2);

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_shortcut_icon_url1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(), "1");
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(), "data1");

  ASSERT_EQ(manifest.shortcuts(1).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).src(), best_shortcut_icon_url2);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).hash(), "2");
  EXPECT_EQ(manifest.shortcuts(1).icons(0).image_data(), "data2");
}

TEST_F(WebApkProtoBuilderTest,
       BuildWebApkProtoWhenWithMultipleShortcutsAndSameIcons) {
  GURL best_shortcut_icon_url = test_server()->GetURL(kBestShortcutIconUrl);
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(best_shortcut_icon_url,
                       BuildTestWebApkIcon(best_shortcut_icon_url, "data1", "1",
                                           {webapk::Image::SHORTCUT_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), std::move(webapk_icons), nullptr /* primary_icon */,
      nullptr /*splash_icon*/, GURL() /*manifest_id*/, GURL() /*app_key*/,
      0x000000 /*dark_theme_color*/, 0x000000 /*dark_background_color*/,
      false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */,
      {GURL(best_shortcut_icon_url), GURL(best_shortcut_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(manifest.shortcuts_size(), 2);

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_shortcut_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(), "1");
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(), "data1");

  ASSERT_EQ(manifest.shortcuts(1).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).src(), best_shortcut_icon_url);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).hash(), "1");
  // This is a duplicate icon, so the data won't be included again.
  EXPECT_EQ(manifest.shortcuts(1).icons(0).image_data(), "");
}

TEST_F(WebApkProtoBuilderTest,
       BuildWebApkProtoSplashIconAndShortcutIconSameUrl) {
  GURL icon_url_1 = test_server()->GetURL("/icon.png");
  GURL best_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(icon_url_1,
                       BuildTestWebApkIcon(icon_url_1, "data1", "1",
                                           {webapk::Image::PRIMARY_ICON}));
  webapk_icons.emplace(best_icon_url,
                       BuildTestWebApkIcon(best_icon_url, "data0", "0",
                                           {webapk::Image::SPLASH_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(icon_url_1), GURL(best_icon_url), std::move(webapk_icons),
      nullptr /* primary_icon */, nullptr /*splash_icon*/,
      GURL() /*manifest_id*/, GURL() /*app_key*/, 0x000000 /*dark_theme_color*/,
      0x000000 /*dark_background_color*/, false /* is_manifest_stale*/,
      true /* is_app_identity_update_supported */, {GURL(best_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());
  ASSERT_EQ(manifest.shortcuts_size(), 1);

  // Check primary icon fields.
  EXPECT_EQ(icon_url_1, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(), "1");
  EXPECT_EQ(manifest.icons(0).image_data(), "data1");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  // Check splash icon fields
  EXPECT_EQ(best_icon_url, manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(), "0");
  EXPECT_EQ(manifest.icons(1).image_data(), "data0");
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(), "0");
  EXPECT_TRUE(manifest.shortcuts(0).icons(0).has_image_data());
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(), "data0");
}

TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoManifestIdAndKey) {
  GURL manifest_id_1 = test_server()->GetURL("/test_id");
  GURL app_key_1 = test_server()->GetURL("/test_key");

  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(), GURL(), std::move(webapk_icons),
                    nullptr /* primary_icon */, nullptr /*splash_icon*/,
                    manifest_id_1, app_key_1, 0x000000 /*dark_theme_color*/,
                    0x000000 /*dark_background_color*/,
                    false /* is_manifest_stale*/,
                    false /* is_app_identity_update_supported */, {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  EXPECT_EQ(webapk_request->app_key(), app_key_1.spec());
  EXPECT_EQ(webapk_request->manifest().id(), manifest_id_1.spec());
}

TEST_F(WebApkProtoBuilderTest, MapContainsEmptyIconUrl) {
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(GURL(""),
                       BuildTestWebApkIcon(GURL(""), "data", "0", {}));

  auto primary_icon =
      BuildTestWebApkIcon(GURL(), "primary_icon_data", std::string(),
                          {webapk::Image::PRIMARY_ICON});
  auto splash_icon = BuildTestWebApkIcon(
      GURL(), "splash_icon_data", std::string(), {webapk::Image::SPLASH_ICON});

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL() /* primary_icon_url */, GURL() /*splash_icon_url*/,
      std::move(webapk_icons), std::move(primary_icon), std::move(splash_icon),
      GURL() /*manifest_id*/, GURL() /*app_key*/, 0x000000 /*dark_theme_color*/,
      0x000000 /*dark_background_color*/, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */, {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  // Checks the request contains primary icon and splash icon with no url and
  // hash.
  EXPECT_EQ(manifest.icons(0).src(), "");
  EXPECT_FALSE(manifest.icons(0).has_hash());
  EXPECT_EQ(manifest.icons(0).image_data(), "primary_icon_data");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  EXPECT_EQ(manifest.icons(1).src(), "");
  EXPECT_FALSE(manifest.icons(0).has_hash());
  EXPECT_EQ(manifest.icons(1).image_data(), "splash_icon_data");
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));
}

TEST_F(WebApkProtoBuilderTest, EmptyPrimaryIconUrlValidSplashIcon) {
  GURL splash_icon_url = test_server()->GetURL(kBestSplashIconUrl);
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(splash_icon_url,
                       BuildTestWebApkIcon(splash_icon_url, "data2", "2",
                                           {webapk::Image::SPLASH_ICON}));

  auto primary_icon =
      BuildTestWebApkIcon(GURL(), "primary_icon_data", std::string(),
                          {webapk::Image::PRIMARY_ICON});
  auto splash_icon =
      BuildTestWebApkIcon(splash_icon_url, "splash_icon_data",
                          "splash_icon_hash", {webapk::Image::SPLASH_ICON});

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(splash_icon_url), std::move(webapk_icons),
      std::move(primary_icon), std::move(splash_icon), GURL() /*manifest_id*/,
      GURL() /*app_key*/, 0x000000 /*dark_theme_color*/,
      0x000000 /*dark_background_color*/, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */, {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  // Check protobuf fields for icons.
  EXPECT_EQ(manifest.icons(0).src(), "");
  EXPECT_FALSE(manifest.icons(0).has_hash());
  EXPECT_EQ(manifest.icons(0).image_data(), "primary_icon_data");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  EXPECT_EQ(manifest.icons(1).src(), splash_icon_url);
  EXPECT_EQ(manifest.icons(1).hash(), "splash_icon_hash");
  EXPECT_EQ(manifest.icons(1).image_data(), "splash_icon_data");
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));
}

TEST_F(WebApkProtoBuilderTest, IconWithoutUrl_Install) {
  // Test primary icon with empty URL.
  std::vector<std::string> icon_urls = {kUnusedIconPath};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(GURL(), BuildTestWebApkIcon(GURL(), "data", "hash", {}));
  webapk_icons.emplace(
      GURL(kUnusedIconPath),
      BuildTestWebApkIcon(GURL(kUnusedIconPath), "data2", "2", {}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconInstallTestSync(GURL(), icon_urls,
                                      std::move(webapk_icons));

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(1, manifest.icons_size());
  // The dummy unused icon has src and hash but no data
  EXPECT_TRUE(manifest.icons(0).has_src());
  EXPECT_TRUE(manifest.icons(0).has_hash());
}

TEST_F(WebApkProtoBuilderTest, IconWithoutUrl_Update) {
  // Test primary icon with empty URL.
  std::vector<std::string> icon_urls = {kUnusedIconPath};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(GURL(), BuildTestWebApkIcon(GURL(), "data", "hash", {}));
  webapk_icons.emplace(
      GURL(kUnusedIconPath),
      BuildTestWebApkIcon(GURL(kUnusedIconPath), "data2", "2", {}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconUpdateTestSync(GURL(), icon_urls, std::move(webapk_icons),
                                     "icon_data", "icon_hash");

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(2, manifest.icons_size());
  // Icon has data and hash but no src.
  EXPECT_FALSE(manifest.icons(0).has_src());
  EXPECT_EQ(manifest.icons(0).hash(), "icon_hash");
  EXPECT_EQ(manifest.icons(0).image_data(), "icon_data");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
  // The dummy unused icon has src and hash but no data
  EXPECT_TRUE(manifest.icons(1).has_src());
  EXPECT_TRUE(manifest.icons(1).has_hash());
}

TEST_F(WebApkProtoBuilderTest, IconUrlNotInListAndNotInHash_Install) {
  // Test primary icon URL is NOT in list and NOT in hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {kUnusedIconPath};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(
      GURL(kUnusedIconPath),
      BuildTestWebApkIcon(GURL(kUnusedIconPath), "data1", "1", {}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconInstallTestSync(test_icon_url, icon_urls,
                                      std::move(webapk_icons));

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(1, manifest.icons_size());
  // The dummy unused icon has src and hash but no data
  EXPECT_TRUE(manifest.icons(0).has_src());
  EXPECT_TRUE(manifest.icons(0).has_hash());
}

TEST_F(WebApkProtoBuilderTest, IconUrlNotInListAndNotInHash_Update) {
  // Test primary icon has URL but NOT in list and NOT in hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {kUnusedIconPath};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(
      GURL(kUnusedIconPath),
      BuildTestWebApkIcon(GURL(kUnusedIconPath), "data1", "1", {}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconUpdateTestSync(test_icon_url, icon_urls,
                                     std::move(webapk_icons), "icon_data",
                                     "icon_hash");

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(2, manifest.icons_size());
  // Icon has data, src and hash
  EXPECT_EQ(manifest.icons(0).src(), test_icon_url.spec());
  EXPECT_EQ(manifest.icons(0).hash(), "icon_hash");
  EXPECT_EQ(manifest.icons(0).image_data(), "icon_data");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
  // The dummy unused icon has src and hash but no data
  EXPECT_TRUE(manifest.icons(1).has_src());
  EXPECT_TRUE(manifest.icons(1).has_hash());
}

TEST_F(WebApkProtoBuilderTest, IconUrlInListNotInHash_Install) {
  // Test primary icon has URL in the urls list but NOT in hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {test_icon_url.spec()};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(
      GURL(kUnusedIconPath),
      BuildTestWebApkIcon(GURL(kUnusedIconPath), "data2", "hash2", {}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconInstallTestSync(test_icon_url, icon_urls,
                                      std::move(webapk_icons));

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  // No icon added because either primary icon or list icons in hash.
  ASSERT_EQ(0, manifest.icons_size());
}

TEST_F(WebApkProtoBuilderTest, IconUrlInListNotInHash_Update) {
  // Test primary icon has URL in the urls list but NOT in hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {test_icon_url.spec()};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(
      GURL(kUnusedIconPath),
      BuildTestWebApkIcon(GURL(kUnusedIconPath), "data2", "hash2", {}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconUpdateTestSync(test_icon_url, icon_urls,
                                     std::move(webapk_icons), "icon_data",
                                     "icon_hash");

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(1, manifest.icons_size());
  // Icon has data, src and hash
  EXPECT_EQ(manifest.icons(0).src(), test_icon_url.spec());
  EXPECT_EQ(manifest.icons(0).hash(), "icon_hash");
  EXPECT_EQ(manifest.icons(0).image_data(), "icon_data");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
}

TEST_F(WebApkProtoBuilderTest, IconUrlNotInListInHash_Install) {
  // Test primary icon has URL NOT in the urls list but in the hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(test_icon_url,
                       BuildTestWebApkIcon(test_icon_url, "data3", "hash3",
                                           {webapk::Image::PRIMARY_ICON}));
  webapk_icons.emplace(
      GURL(kUnusedIconPath),
      BuildTestWebApkIcon(GURL(kUnusedIconPath), "data2", "hash2", {}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconInstallTestSync(test_icon_url, icon_urls,
                                      std::move(webapk_icons));

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(1, manifest.icons_size());
  // Primary icon has data, src and hash
  EXPECT_EQ(manifest.icons(0).src(), test_icon_url.spec());
  EXPECT_EQ(manifest.icons(0).hash(), "hash3");
  EXPECT_EQ(manifest.icons(0).image_data(), "data3");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
}

TEST_F(WebApkProtoBuilderTest, IconUrlNotInListInHash_Update) {
  // Test primary icon has URL NOT in the urls list but in the hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(test_icon_url,
                       BuildTestWebApkIcon(test_icon_url, "data3", "hash3",
                                           {webapk::Image::PRIMARY_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconUpdateTestSync(test_icon_url, icon_urls,
                                     std::move(webapk_icons), "icon_data",
                                     "icon_hash");

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(1, manifest.icons_size());
  // Icon has data, src and hash
  EXPECT_EQ(manifest.icons(0).src(), test_icon_url.spec());
  EXPECT_EQ(manifest.icons(0).hash(), "icon_hash");
  EXPECT_EQ(manifest.icons(0).image_data(), "icon_data");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
}

TEST_F(WebApkProtoBuilderTest, IconUrlInListAndHash_Install) {
  // Test primary icon has URL in list and hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {test_icon_url.spec()};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(test_icon_url,
                       BuildTestWebApkIcon(test_icon_url, "data4", "hash4",
                                           {webapk::Image::PRIMARY_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconInstallTestSync(test_icon_url, icon_urls,
                                      std::move(webapk_icons));

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(1, manifest.icons_size());
  // Icon has data, src and hash
  EXPECT_EQ(manifest.icons(0).src(), test_icon_url.spec());
  EXPECT_EQ(manifest.icons(0).hash(), "hash4");
  EXPECT_EQ(manifest.icons(0).image_data(), "data4");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
}

TEST_F(WebApkProtoBuilderTest, IconUrlInListAndHash_Update) {
  // Test primary icon has URL in list and hashmap
  GURL test_icon_url = test_server()->GetURL(kBestPrimaryIconUrl);
  std::vector<std::string> icon_urls = {test_icon_url.spec()};
  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  webapk_icons.emplace(test_icon_url,
                       BuildTestWebApkIcon(test_icon_url, "data4", "hash4",
                                           {webapk::Image::PRIMARY_ICON}));

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildForIconUpdateTestSync(test_icon_url, icon_urls,
                                     std::move(webapk_icons), "icon_data",
                                     "icon_hash");

  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);
  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(1, manifest.icons_size());
  // Icon has data, src and hash
  EXPECT_EQ(manifest.icons(0).src(), test_icon_url.spec());
  EXPECT_EQ(manifest.icons(0).hash(), "icon_hash");
  EXPECT_EQ(manifest.icons(0).image_data(), "icon_data");
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));
}

TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoDarkThemeAndBackground) {
  std::optional<SkColor> dark_theme_color = 0x000000;
  std::optional<SkColor> dark_background_color = 0x888888;
  std::string dark_theme_color_expected = "rgba(0,0,0,0)";
  std::string dark_background_color_expected = "rgba(136,136,136,0)";

  std::map<GURL, std::unique_ptr<webapps::WebappIcon>> webapk_icons;
  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), std::move(webapk_icons), nullptr /* primary_icon */,
      nullptr /*splash_icon*/, GURL() /*manifest_id*/, GURL() /*app_key*/,
      dark_theme_color, dark_background_color, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */, {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  EXPECT_EQ(manifest.dark_theme_color(), dark_theme_color_expected);
  EXPECT_EQ(manifest.dark_background_color(), dark_background_color_expected);
}
