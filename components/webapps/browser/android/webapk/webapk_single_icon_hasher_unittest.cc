// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapk/webapk_single_icon_hasher.h"

#include <string>

#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/webapps/browser/android/webapk/webapk_icons_hasher.h"
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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webapps {
namespace {

// Murmur2 hash for |icon_url| in Success test.
const char kIconMurmur2Hash[] = "2081059568551351877";

// Runs WebApkSingleIconHasher and blocks till the murmur2 hash is computed.
class WebApkIconHasherRunner {
 public:
  WebApkIconHasherRunner() = default;
  ~WebApkIconHasherRunner() = default;
  WebApkIconHasherRunner(const WebApkIconHasherRunner&) = delete;
  WebApkIconHasherRunner& operator=(const WebApkIconHasherRunner&) = delete;

  void Run(network::mojom::URLLoaderFactory* url_loader_factory,
           content::WebContents* web_contents,
           const GURL& icon_url) {
    icon_ = std::make_unique<WebappIcon>(icon_url);
    hasher_ = std::make_unique<WebApkSingleIconHasher>(
        WebApkIconsHasher::PassKeyForTesting(), url_loader_factory,
        web_contents->GetWeakPtr(), url::Origin::Create(icon_url),
        /*timeout_ms=*/300, icon_.get(),
        base::BindOnce(&WebApkIconHasherRunner::OnCompleted,
                       base::Unretained(this)));

    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnCompleted() {
    if (!on_completed_callback_.is_null())
      std::move(on_completed_callback_).Run();
  }

  const WebappIcon* icon() const { return icon_.get(); }

 private:
  // Fake factory that can be primed to return fake data.
  network::TestURLLoaderFactory test_url_loader_factory_;

  // Called once the Murmur2 hash is taken.
  base::OnceClosure on_completed_callback_;

  // Holds icon data and computed hash.
  std::unique_ptr<WebappIcon> icon_;

  std::unique_ptr<WebApkSingleIconHasher> hasher_;
};

}  // anonymous namespace

class WebApkSingleIconHasherTest : public ::testing::Test {
 public:
  WebApkSingleIconHasherTest() {
    web_contents_ = web_contents_factory_.CreateWebContents(&browser_context_);
  }
  ~WebApkSingleIconHasherTest() override = default;
  WebApkSingleIconHasherTest(const WebApkSingleIconHasherTest&) = delete;
  WebApkSingleIconHasherTest&
  operator=(const WebApkSingleIconHasherTest&) = delete;

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

TEST_F(WebApkSingleIconHasherTest, Success) {
  GURL icon_url("http://www.google.com/chrome/test/data/android/google.png");
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
  test_url_loader_factory()->AddResponse(icon_url, std::move(head), icon_data,
                                         status);

  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), web_contents(), icon_url);
  EXPECT_EQ(kIconMurmur2Hash, runner.icon()->hash());
  EXPECT_FALSE(runner.icon()->unsafe_data().empty());
}

TEST_F(WebApkSingleIconHasherTest, DataUri) {
  GURL icon_url(
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA"
      "AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO"
      "9TXL0Y4OHwAAAABJRU5ErkJggg==");
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), web_contents(), icon_url);
  EXPECT_EQ("536500236142107998", runner.icon()->hash());
  EXPECT_FALSE(runner.icon()->unsafe_data().empty());
}

TEST_F(WebApkSingleIconHasherTest, SVGImage) {
  GURL icon_url("http://www.google.com/chrome/test/data/android/icon.svg");
  base::FilePath source_path;
  base::FilePath icon_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path));
  icon_path = source_path.AppendASCII("components")
                  .AppendASCII("test")
                  .AppendASCII("data")
                  .AppendASCII("webapps")
                  .AppendASCII("icon.svg");
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 200 OK\nContent-type: image/svg+xml\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "image/svg+xml";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = icon_data.size();
  test_url_loader_factory()->AddResponse(icon_url, std::move(head), icon_data,
                                         status);

  auto icon = std::make_unique<WebappIcon>(icon_url);
  auto hasher = std::make_unique<WebApkSingleIconHasher>(
      WebApkIconsHasher::PassKeyForTesting(), test_url_loader_factory(),
      web_contents()->GetWeakPtr(), url::Origin::Create(icon_url),
      /*timeout_ms=*/300, icon.get(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SkBitmap dummy_bitmap;
  dummy_bitmap.allocN32Pixels(10, 10);
  dummy_bitmap.setImmutable();
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->TestDidDownloadImage(
                      icon_url, 200, std::vector<SkBitmap>{dummy_bitmap},
                      std::vector<gfx::Size>{gfx::Size(10, 10)}));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("12895188166704127516", icon->hash());
  EXPECT_FALSE(icon->unsafe_data().empty());
}

TEST_F(WebApkSingleIconHasherTest, WebpImage) {
  GURL icon_url("http://www.google.com/chrome/test/data/android/splash.webp");
  base::FilePath source_path;
  base::FilePath icon_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path));
  icon_path = source_path.AppendASCII("components")
                  .AppendASCII("test")
                  .AppendASCII("data")
                  .AppendASCII("webapps")
                  .AppendASCII("splash.webp");
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 200 OK\nContent-type: image/webp\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "image/webp";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = icon_data.size();
  test_url_loader_factory()->AddResponse(icon_url, std::move(head), icon_data,
                                         status);

  auto icon = std::make_unique<WebappIcon>(icon_url);
  auto hasher = std::make_unique<WebApkSingleIconHasher>(
      WebApkIconsHasher::PassKeyForTesting(), test_url_loader_factory(),
      web_contents()->GetWeakPtr(), url::Origin::Create(icon_url),
      /*timeout_ms=*/300, icon.get(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SkBitmap dummy_bitmap;
  dummy_bitmap.allocN32Pixels(10, 10);
  dummy_bitmap.setImmutable();
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->TestDidDownloadImage(
                      icon_url, 200, std::vector<SkBitmap>{dummy_bitmap},
                      std::vector<gfx::Size>{gfx::Size(10, 10)}));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("2160198985949168049", icon->hash());
  EXPECT_FALSE(icon->unsafe_data().empty());
}

TEST_F(WebApkSingleIconHasherTest, Favicon) {
  GURL icon_url("http://www.google.com/chrome/test/data/android/favicon_1.ico");
  base::FilePath source_path;
  base::FilePath icon_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path));
  icon_path = source_path.AppendASCII("components")
                  .AppendASCII("test")
                  .AppendASCII("data")
                  .AppendASCII("webapps")
                  .AppendASCII("favicon_1.ico");
  std::string icon_data;
  ASSERT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  auto head = network::mojom::URLResponseHead::New();
  std::string headers(
      "HTTP/1.1 200 OK\nContent-type: image/vnd.microsoft.icon\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "image/vnd.microsoft.icon";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = icon_data.size();
  test_url_loader_factory()->AddResponse(icon_url, std::move(head), icon_data,
                                         status);

  auto icon = std::make_unique<WebappIcon>(icon_url);
  auto hasher = std::make_unique<WebApkSingleIconHasher>(
      WebApkIconsHasher::PassKeyForTesting(), test_url_loader_factory(),
      web_contents()->GetWeakPtr(), url::Origin::Create(icon_url),
      /*timeout_ms=*/300, icon.get(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SkBitmap dummy_bitmap;
  dummy_bitmap.allocN32Pixels(10, 10);
  dummy_bitmap.setImmutable();
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->TestDidDownloadImage(
                      icon_url, 200, std::vector<SkBitmap>{dummy_bitmap},
                      std::vector<gfx::Size>{gfx::Size(10, 10)}));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("14576046868078225019", icon->hash());
  EXPECT_FALSE(icon->unsafe_data().empty());
}

TEST_F(WebApkSingleIconHasherTest, DataUriInvalid) {
  GURL icon_url("data:image/png;base64");
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), web_contents(), icon_url);
  EXPECT_TRUE(runner.icon()->hash().empty());
  EXPECT_TRUE(runner.icon()->unsafe_data().empty());
}

TEST_F(WebApkSingleIconHasherTest, InvalidUrl) {
  GURL icon_url("http::google.com");
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), web_contents(), icon_url);
  EXPECT_TRUE(runner.icon()->hash().empty());
  EXPECT_TRUE(runner.icon()->unsafe_data().empty());
}

TEST_F(WebApkSingleIconHasherTest, DownloadTimedOut) {
  std::string icon_url = "http://www.google.com/timeout";
  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), web_contents(), GURL(icon_url));
  EXPECT_TRUE(runner.icon()->hash().empty());
  EXPECT_TRUE(runner.icon()->unsafe_data().empty());
}

// Test that the hash callback is called with an empty string if an HTTP error
// prevents the icon URL from being fetched.
TEST_F(WebApkSingleIconHasherTest, HTTPError) {
  std::string icon_url = "http://www.google.com/404";
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 404 Not Found\nContent-type: text/html\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "text/html";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = 0;
  test_url_loader_factory()->AddResponse(GURL(icon_url), std::move(head), "",
                                         status);

  WebApkIconHasherRunner runner;
  runner.Run(test_url_loader_factory(), web_contents(), GURL(icon_url));
  EXPECT_TRUE(runner.icon()->hash().empty());
  EXPECT_TRUE(runner.icon()->unsafe_data().empty());
}

}  // namespace webapps
