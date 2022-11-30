// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_url_loader.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class WebAppUrlLoaderTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    loader_ = std::make_unique<WebAppUrlLoader>();
  }

  void TearDown() override {
    loader_.reset();
    WebAppTest::TearDown();
  }

  content::WebContentsTester& web_contents_tester() {
    // RenderViewHostTestHarness always contains TestWebContents. See comments
    // in web_contents_tester.h.
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    DCHECK(web_contents_tester);
    return *web_contents_tester;
  }

  WebAppUrlLoader& loader() {
    DCHECK(loader_);
    return *loader_;
  }

  WebAppUrlLoader::Result LoadUrl(const GURL& desired, const GURL& actual) {
    WebAppUrlLoader::Result result;

    base::RunLoop run_loop;
    loader().LoadUrl(desired, web_contents(),
                     WebAppUrlLoader::UrlComparison::kExact,
                     base::BindLambdaForTesting([&](WebAppUrlLoader::Result r) {
                       result = r;
                       run_loop.Quit();
                     }));
    web_contents_tester().TestDidFinishLoad(actual);
    run_loop.Run();

    return result;
  }

 private:
  std::unique_ptr<WebAppUrlLoader> loader_;
};

TEST_F(WebAppUrlLoaderTest, Url1Redirected_ThenUrl2Loaded) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  EXPECT_EQ(WebAppUrlLoader::Result::kRedirectedUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url2));

  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url2, /*actual=*/url2));
}

TEST_F(WebAppUrlLoaderTest, Url1DidFailLoad_ThenUrl2Loaded) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  base::RunLoop run_loop;
  loader().LoadUrl(
      url1, web_contents(), WebAppUrlLoader::UrlComparison::kExact,
      base::BindLambdaForTesting([&](WebAppUrlLoader::Result result) {
        EXPECT_EQ(WebAppUrlLoader::Result::kFailedUnknownReason, result);
        run_loop.Quit();
      }));
  web_contents_tester().TestDidFailLoadWithError(url1, /*error_code=*/1);
  run_loop.Run();

  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url2, /*actual=*/url2));
}

TEST_F(WebAppUrlLoaderTest, PrepareForLoad_ExcessiveDidFinishLoad) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  // Expect successful navigation to url1.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url1));

  // Simulate an excessive DidFinishLoad for url1 caused by active javascript
  // while in PrepareForLoad state. PrepareForLoad() acts as a barrier here:
  // it's flushing all url1-related noisy events so url2 loading will start
  // clean later.
  {
    base::RunLoop run_loop;
    loader().PrepareForLoad(
        web_contents(),
        base::BindLambdaForTesting([&](WebAppUrlLoader::Result result) {
          EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
          run_loop.Quit();
        }));
    web_contents_tester().TestDidFinishLoad(url1);
    web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
    run_loop.Run();
  }

  // Expect successful navigation to url2.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url2, /*actual=*/url2));
}

TEST_F(WebAppUrlLoaderTest, PrepareForLoad_ExcessiveDidFailLoad) {
  const GURL url1{"https://example.com"};
  const GURL url2{"https://example.org"};

  // Expect successful navigation to url1.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url1, /*actual=*/url1));

  // Simulate an excessive DidFailLoad for url1 caused by active javascript
  // while in PrepareForLoad state. PrepareForLoad() acts as a barrier here:
  // it's flushing all url1-related noisy events so url2 loading will start
  // clean later.
  {
    base::RunLoop run_loop;
    loader().PrepareForLoad(
        web_contents(),
        base::BindLambdaForTesting([&](WebAppUrlLoader::Result result) {
          EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded, result);
          run_loop.Quit();
        }));
    web_contents_tester().TestDidFailLoadWithError(url1, /*error_code=*/1);
    web_contents_tester().TestDidFinishLoad(GURL{url::kAboutBlankURL});
    run_loop.Run();
  }

  // Expect successful navigation to url2.
  EXPECT_EQ(WebAppUrlLoader::Result::kUrlLoaded,
            LoadUrl(/*desired=*/url2, /*actual=*/url2));
}

}  // namespace web_app
