// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_proto_builder.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

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

}  // namespace

// Builds WebApk proto and blocks till done.
class BuildProtoRunner {
 public:
  BuildProtoRunner() {}

  BuildProtoRunner(const BuildProtoRunner&) = delete;
  BuildProtoRunner& operator=(const BuildProtoRunner&) = delete;

  ~BuildProtoRunner() {}

  void BuildSync(const GURL& best_primary_icon_url,
                 const GURL& splash_image_url,
                 std::map<std::string, webapps::WebApkIconHasher::Icon>
                     icon_url_to_murmur2_hash,
                 const std::string& primary_icon_data,
                 const std::string& splash_icon_data,
                 const GURL& manifest_id,
                 const GURL& app_key,
                 bool is_manifest_stale,
                 bool is_app_identity_update_supported,
                 const std::vector<GURL>& best_shortcut_icon_urls) {
    webapps::ShortcutInfo info(GURL::EmptyGURL());
    info.manifest_id = manifest_id;
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

    webapps::BuildProto(info, app_key, primary_icon_data,
                        false /* is_primary_icon_maskable */, splash_icon_data,
                        "" /* package_name */, "" /* version */,
                        std::move(icon_url_to_murmur2_hash), is_manifest_stale,
                        is_app_identity_update_supported,
                        base::BindOnce(&BuildProtoRunner::OnBuiltWebApkProto,
                                       base::Unretained(this)));

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  webapk::WebApk* GetWebApkRequest() { return webapk_request_.get(); }

 private:
  // Called when the |webapk_request_| is populated.
  void OnBuiltWebApkProto(std::unique_ptr<std::string> serialized_proto) {
    webapk_request_ = std::make_unique<webapk::WebApk>();
    webapk_request_->ParseFromString(*serialized_proto);
    std::move(on_completed_callback_).Run();
  }

  // The populated webapk::WebApk.
  std::unique_ptr<webapk::WebApk> webapk_request_;

  // Called after the |webapk_request_| is built.
  base::OnceClosure on_completed_callback_;
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

  ~WebApkProtoBuilderTest() override {}

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
  std::string icon_url_1 = test_server()->GetURL("/icon1.png").spec();
  std::string icon_url_2 = test_server()->GetURL("/icon2.png").spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
  icon_url_to_murmur2_hash[icon_url_2] = {"data2", "2"};

  std::string primary_icon_data = "data3";
  std::string splash_icon_data = "data4";

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(), GURL(), std::move(icon_url_to_murmur2_hash),
                    primary_icon_data, splash_icon_data, GURL() /*manifest_id*/,
                    GURL() /*app_key*/, true /* is_manifest_stale */,
                    true /* is_app_identity_update_supported */, {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  EXPECT_EQ("", manifest.icons(0).src());
  EXPECT_FALSE(manifest.icons(0).has_hash());
  EXPECT_EQ(manifest.icons(0).image_data(), primary_icon_data);

  EXPECT_EQ("", manifest.icons(1).src());
  EXPECT_FALSE(manifest.icons(1).has_hash());
  EXPECT_EQ(manifest.icons(1).image_data(), splash_icon_data);

  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest.
TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoWhenManifestIsAvailable) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_primary_icon_url =
      test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::string best_splash_icon_url =
      test_server()->GetURL(kBestSplashIconUrl).spec();
  std::string best_shortcut_icon_url =
      test_server()->GetURL(kBestShortcutIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data0", "0"};
  icon_url_to_murmur2_hash[best_primary_icon_url] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_splash_icon_url] = {"data2", "2"};
  icon_url_to_murmur2_hash[best_shortcut_icon_url] = {"data3", "3"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(best_primary_icon_url), GURL(best_splash_icon_url),
                    icon_url_to_murmur2_hash, "" /* primary_icon_data */,
                    "" /* splash_icon_data */, GURL() /*manifest_id*/,
                    GURL() /*app_key*/, false /* is_manifest_stale*/,
                    false /* is_app_identity_update_supported */,
                    {GURL(best_shortcut_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  // Check protobuf fields for kBestPrimaryIconUrl.
  EXPECT_EQ(best_primary_icon_url, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(),
            icon_url_to_murmur2_hash[best_primary_icon_url].hash);
  EXPECT_EQ(manifest.icons(0).image_data(),
            icon_url_to_murmur2_hash[best_primary_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  // Check protobuf fields for kBestSplashIconUrl.
  EXPECT_EQ(best_splash_icon_url, manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(),
            icon_url_to_murmur2_hash[best_splash_icon_url].hash);
  EXPECT_EQ(manifest.icons(1).image_data(),
            icon_url_to_murmur2_hash[best_splash_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_shortcut_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].unsafe_data);
}

// Tests a WebApk install or update request is built properly when the Chrome
// knows the best icon URL of a site after fetching its Web Manifest, and
// primary icon and splash icon share the same URL.
TEST_F(WebApkProtoBuilderTest,
       BuildWebApkProtoPrimaryIconAndSplashIconSameUrl) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_icon_url = test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_icon_url] = {"data0", "0"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(best_icon_url), GURL(best_icon_url), icon_url_to_murmur2_hash,
      "" /* primary_icon_data */, "" /* splash_icon_data */,
      GURL() /*manifest_id*/, GURL() /*app_key*/, false /* is_manifest_stale*/,
      false /* is_app_identity_update_supported */, {GURL(best_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());

  // Check protobuf fields for icons.
  EXPECT_EQ(best_icon_url, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.icons(0).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON,
                                   webapk::Image::SPLASH_ICON));

  EXPECT_EQ(best_icon_url, manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.icons(1).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON,
                                   webapk::Image::SPLASH_ICON));

  // Check protobuf fields for unused icon.
  EXPECT_EQ(kUnusedIconPath, manifest.icons(2).src());
  EXPECT_FALSE(manifest.icons(2).has_hash());
  EXPECT_FALSE(manifest.icons(2).has_image_data());

  // Check shortcut fields.
  ASSERT_EQ(manifest.shortcuts_size(), 1);
  ASSERT_EQ(manifest.shortcuts(0).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).src(), best_icon_url);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
}

TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoWhenWithMultipleShortcuts) {
  std::string best_shortcut_icon_url1 =
      test_server()->GetURL(kBestShortcutIconUrl).spec();
  std::string best_shortcut_icon_url2 =
      test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[best_shortcut_icon_url1] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_shortcut_icon_url2] = {"data2", "2"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), icon_url_to_murmur2_hash, "" /* primary_icon_data */,
      "" /* splash_icon_data */, GURL() /*manifest_id*/, GURL() /*app_key*/,
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
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url1].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url1].unsafe_data);

  ASSERT_EQ(manifest.shortcuts(1).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).src(), best_shortcut_icon_url2);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url2].hash);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url2].unsafe_data);
}

TEST_F(WebApkProtoBuilderTest,
       BuildWebApkProtoWhenWithMultipleShortcutsAndSameIcons) {
  std::string best_shortcut_icon_url =
      test_server()->GetURL(kBestShortcutIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[best_shortcut_icon_url] = {"data1", "1"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(), GURL(), icon_url_to_murmur2_hash, "" /* primary_icon_data */,
      "" /* splash_icon_data */, GURL() /*manifest_id*/, GURL() /*app_key*/,
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
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].hash);
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].unsafe_data);

  ASSERT_EQ(manifest.shortcuts(1).icons_size(), 1);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).src(), best_shortcut_icon_url);
  EXPECT_EQ(manifest.shortcuts(1).icons(0).hash(),
            icon_url_to_murmur2_hash[best_shortcut_icon_url].hash);
  // This is a duplicate icon, so the data won't be included again.
  EXPECT_EQ(manifest.shortcuts(1).icons(0).image_data(), "");
}

TEST_F(WebApkProtoBuilderTest,
       BuildWebApkProtoSplashIconAndShortcutIconSameUrl) {
  std::string icon_url_1 = test_server()->GetURL("/icon.png").spec();
  std::string best_icon_url = test_server()->GetURL(kBestPrimaryIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[icon_url_1] = {"data1", "1"};
  icon_url_to_murmur2_hash[best_icon_url] = {"data0", "0"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(
      GURL(icon_url_1), GURL(best_icon_url), icon_url_to_murmur2_hash,
      "" /* primary_icon_data */, "" /* splash_icon_data */,
      GURL() /*manifest_id*/, GURL() /*app_key*/, false /* is_manifest_stale*/,
      true /* is_app_identity_update_supported */, {GURL(best_icon_url)});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  webapk::WebAppManifest manifest = webapk_request->manifest();
  ASSERT_EQ(3, manifest.icons_size());
  ASSERT_EQ(manifest.shortcuts_size(), 1);

  // Check primary icon fields.
  EXPECT_EQ(icon_url_1, manifest.icons(0).src());
  EXPECT_EQ(manifest.icons(0).hash(),
            icon_url_to_murmur2_hash[icon_url_1].hash);
  EXPECT_EQ(manifest.icons(0).image_data(),
            icon_url_to_murmur2_hash[icon_url_1].unsafe_data);
  EXPECT_THAT(manifest.icons(0).usages(),
              testing::ElementsAre(webapk::Image::PRIMARY_ICON));

  // Check splash icon fields
  EXPECT_EQ(best_icon_url, manifest.icons(1).src());
  EXPECT_EQ(manifest.icons(1).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_EQ(manifest.icons(1).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
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
  EXPECT_EQ(manifest.shortcuts(0).icons(0).hash(),
            icon_url_to_murmur2_hash[best_icon_url].hash);
  EXPECT_TRUE(manifest.shortcuts(0).icons(0).has_image_data());
  EXPECT_EQ(manifest.shortcuts(0).icons(0).image_data(),
            icon_url_to_murmur2_hash[best_icon_url].unsafe_data);
}

TEST_F(WebApkProtoBuilderTest, BuildWebApkProtoManifestIdAndKey) {
  base::test::ScopedFeatureList feature_list(
      webapps::features::kWebApkUniqueId);
  GURL manifest_id_1 = test_server()->GetURL("/test_id");
  GURL app_key_1 = test_server()->GetURL("/test_key");

  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(), GURL(), icon_url_to_murmur2_hash,
                    "" /* primary_icon_data */, "" /* splash_icon_data */,
                    manifest_id_1, app_key_1, false /* is_manifest_stale*/,
                    false /* is_app_identity_update_supported */, {});
  webapk::WebApk* webapk_request = runner->GetWebApkRequest();
  ASSERT_NE(nullptr, webapk_request);

  EXPECT_EQ(webapk_request->app_key(), app_key_1.spec());
  EXPECT_EQ(webapk_request->manifest().id(), manifest_id_1.spec());
}

TEST_F(WebApkProtoBuilderTest, MapContainsEmptyIconUrl) {
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[""] = {"data", "0"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL() /* primary_icon_url */, GURL() /*splash_icon_url*/,
                    icon_url_to_murmur2_hash, "primary_icon_data",
                    "splash_icon_data", GURL() /*manifest_id*/,
                    GURL() /*app_key*/, false /* is_manifest_stale*/,
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
  std::string splash_icon_url =
      test_server()->GetURL(kBestSplashIconUrl).spec();
  std::map<std::string, webapps::WebApkIconHasher::Icon>
      icon_url_to_murmur2_hash;
  icon_url_to_murmur2_hash[splash_icon_url] = {"data2", "2"};

  std::unique_ptr<BuildProtoRunner> runner = CreateBuildProtoRunner();
  runner->BuildSync(GURL(), GURL(splash_icon_url), icon_url_to_murmur2_hash,
                    "primary_icon_data", "splash_icon_data",
                    GURL() /*manifest_id*/, GURL() /*app_key*/,
                    false /* is_manifest_stale*/,
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
  EXPECT_EQ(manifest.icons(1).hash(),
            icon_url_to_murmur2_hash[splash_icon_url].hash);
  EXPECT_EQ(manifest.icons(1).image_data(), "splash_icon_data");
  EXPECT_THAT(manifest.icons(1).usages(),
              testing::ElementsAre(webapk::Image::SPLASH_ICON));
}
