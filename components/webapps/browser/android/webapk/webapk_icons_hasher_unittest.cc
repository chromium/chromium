// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapp_icon.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webapps {
namespace {

// Murmur2 hash for |icon_url| in Success test.
const char kIconMurmur2Hash[] = "2081059568551351877";
const char kFallbackIconMurmur2Hash[] = "16639143771325572109";

struct TestShortcutInfo : public ShortcutInfo {
 public:
  explicit TestShortcutInfo(const GURL& url, const std::set<GURL>& icon_urls)
      : ShortcutInfo(url), icon_urls_(icon_urls) {
    best_primary_icon_url = *icon_urls.begin();
  }
  ~TestShortcutInfo() override = default;

  std::map<GURL, std::unique_ptr<WebappIcon>> GetWebApkIcons() const override {
    std::map<GURL, std::unique_ptr<WebappIcon>> webapk_icons;
    for (auto icon_url : icon_urls_) {
      webapk_icons.emplace(icon_url, std::make_unique<WebappIcon>(icon_url));
    }
    return webapk_icons;
  }

 private:
  std::set<GURL> icon_urls_;
};

}  // anonymous namespace

class WebApkIconsHasherTest : public ::testing::Test {
 public:
  WebApkIconsHasherTest()
      : web_contents_(
            web_contents_factory_.CreateWebContents(&browser_context_)) {}
  ~WebApkIconsHasherTest() override = default;
  WebApkIconsHasherTest(const WebApkIconsHasherTest&) = delete;
  WebApkIconsHasherTest& operator=(const WebApkIconsHasherTest&) = delete;

  std::map<GURL, std::unique_ptr<WebappIcon>> RunMultiple(
      network::mojom::URLLoaderFactory* url_loader_factory,
      content::WebContents* web_contents,
      const std::set<GURL>& icon_urls) {
    auto test_shortcut_info = TestShortcutInfo(GURL("example.com"), icon_urls);

    std::map<GURL, std::unique_ptr<WebappIcon>> result;
    base::RunLoop run_loop;
    auto icons_hasher = std::make_unique<WebApkIconsHasher>();
    icons_hasher->DownloadAndComputeMurmur2Hash(
        url_loader_factory, web_contents->GetWeakPtr(),
        url::Origin::Create(*icon_urls.begin()), test_shortcut_info,
        gfx::test::CreateBitmap(1, SK_ColorRED),
        base::BindLambdaForTesting(
            [&](std::map<GURL, std::unique_ptr<WebappIcon>> icons) {
              ASSERT_TRUE(!icons.empty());
              result = std::move(icons);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

 protected:
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  content::TestBrowserContext browser_context_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
};

TEST_F(WebApkIconsHasherTest, MultipleIconUrls) {
  std::string icon_url1_string =
      "http://www.google.com/chrome/test/data/android/google.png";
  base::FilePath source_path;
  base::FilePath icon_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path));
  icon_path = source_path.AppendASCII("components")
                  .AppendASCII("test")
                  .AppendASCII("data")
                  .AppendASCII("webapps")
                  .AppendASCII("google.png");
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 200 OK\nContent-type: image/png\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = icon_data.size();
  test_url_loader_factory()->AddResponse(GURL(icon_url1_string),
                                         std::move(head), icon_data, status);

  GURL icon_url1(icon_url1_string);
  GURL icon_url2(
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA"
      "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO"
      "9TXL0Y4OHwAAAABJRU5ErkJggg==");

  {
    auto result =
        RunMultiple(test_url_loader_factory(), web_contents(), {icon_url1});
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result.at(icon_url1)->hash(), kIconMurmur2Hash);
    EXPECT_FALSE(result.at(icon_url1)->unsafe_data().empty());
  }

  {
    auto result = RunMultiple(test_url_loader_factory(), web_contents(),
                              {icon_url1, icon_url2});
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result.at(icon_url1)->hash(), kIconMurmur2Hash);
    EXPECT_FALSE(result.at(icon_url1)->unsafe_data().empty());

    EXPECT_EQ(result.at(icon_url2)->hash(), "536500236142107998");
    EXPECT_FALSE(result.at(icon_url2)->unsafe_data().empty());
  }
}

TEST_F(WebApkIconsHasherTest, PrimaryIconFallbackToEncodeBitmap) {
  std::string icon_url1_string =
      "http://www.google.com/chrome/test/data/android/google.png";
  base::FilePath source_path;
  base::FilePath icon_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path));
  icon_path = source_path.AppendASCII("components")
                  .AppendASCII("test")
                  .AppendASCII("data")
                  .AppendASCII("webapps")
                  .AppendASCII("bad_icon.png");
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 200 OK\nContent-type: image/png\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = icon_data.size();
  test_url_loader_factory()->AddResponse(GURL(icon_url1_string),
                                         std::move(head), icon_data, status);

  GURL icon_url1(icon_url1_string);

  auto result =
      RunMultiple(test_url_loader_factory(), web_contents(), {icon_url1});
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.at(icon_url1)->hash(), kFallbackIconMurmur2Hash);
  EXPECT_TRUE(result.at(icon_url1)->has_unsafe_data());
}

}  // namespace webapps
