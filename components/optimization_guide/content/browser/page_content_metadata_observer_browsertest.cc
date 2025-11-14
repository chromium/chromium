// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"
#include "ui/display/display_switches.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

base::FilePath GetTestDataDir() {
  return base::FilePath{
      FILE_PATH_LITERAL("components/test/data/optimization_guide")};
}

class PageContentMetadataObserverBrowserTest
    : public content::ContentBrowserTest {
 public:
  PageContentMetadataObserverBrowserTest() = default;
  ~PageContentMetadataObserverBrowserTest() override = default;

  content::WebContents* GetWebContents() { return shell()->web_contents(); }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());
    https_server_->SetCertHostnames({"a.com", "b.com", "c.com"});

    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
  }

  bool LoadPage(GURL url) {
    callback_waiter_.Clear();
    return content::NavigateToURL(GetWebContents(), url);
  }

  const std::vector<std::string> names_ = {"author", "subject"};

  void CreateObserver() {
    observer_ = std::make_unique<PageContentMetadataObserver>(
        GetWebContents(), names_,
        base::BindRepeating(
            &PageContentMetadataObserverBrowserTest::OnMetaTagsChanged,
            base::Unretained(this)));
  }

  void OnMetaTagsChanged(blink::mojom::PageMetadataPtr page_metadata) {
    page_metadata_ = std::move(page_metadata);
    // This may be called multiple times in some tests. Only signal the waiter
    // if it is not already ready to avoid crashing the TestFuture. The test
    // will check the latest value of `page_metadata_` when it wakes up.
    if (!callback_waiter_.IsReady()) {
      callback_waiter_.SetValue(true);
    }
  }

  bool ProcessPendingIPC() {
    // Execute a script to ensure all pending IPCs from the renderer have been
    // processed. By the time this returns, the meta tags callback would have
    // been called if it was going to be.
    return content::ExecJs(GetWebContents(), "void(0);");
  }

  void WaitForPageLoadedAndIPCs() {
    // Wait for the page and all subframes to load. This is important for tests
    // with cross-origin iframes.
    content::WaitForLoadStop(GetWebContents());
    ProcessPendingIPC();
  }

  void WaitForCallback() {
    ASSERT_TRUE(callback_waiter_.Wait());
    callback_waiter_.Clear();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  blink::mojom::PageMetadataPtr& page_metadata() { return page_metadata_; }

  std::unique_ptr<PageContentMetadataObserver> observer_;
  base::test::TestFuture<bool> callback_waiter_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  blink::mojom::PageMetadataPtr page_metadata_;
};

IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       MetaTagsAreObserved) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));
  CreateObserver();
  ASSERT_TRUE(callback_waiter_.Wait());

  blink::mojom::PageMetadataPtr& metadata = page_metadata();
  EXPECT_EQ(metadata->frame_metadata.size(), 1u);
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->name, "author");
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->content, "Gary");

  observer_.reset();
}

// TODO(https://crbug.com/455915204): Test is flaky on android-arm64-tests.
// TODO(https://crbug.com/455816130): Test is flaky on linux tests.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
#define MAYBE_NoMetaTags DISABLED_NoMetaTags
#else
#define MAYBE_NoMetaTags NoMetaTags
#endif
IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       MAYBE_NoMetaTags) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/simple.html")));
  CreateObserver();

  WaitForPageLoadedAndIPCs();

  EXPECT_FALSE(callback_waiter_.IsReady());
}

// TODO(crbug.com/440240260): This test flakes frequently on debug / arm64
// builders.
IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       DISABLED_MetaTagsAreObservedInMultipleFrames) {
  ASSERT_TRUE(LoadPage(
      https_server()->GetURL("a.com", "/meta_tags_in_multiple_frames.html")));
  CreateObserver();

  // Wait until we have received metadata from all frames. The callback will be
  // called multiple times, once for each frame that has meta tags.
  while (true) {
    ASSERT_TRUE(callback_waiter_.Wait());
    if (page_metadata()->frame_metadata.size() == 5) {
      break;
    }
    callback_waiter_.Clear();
  }

  // Verify that the meta tags were observed in all frames that have them.
  blink::mojom::PageMetadataPtr& metadata = page_metadata();
  EXPECT_EQ(metadata->frame_metadata.size(), 5u);

  // Main frame.
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->name, "author");
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->content, "George");

  // The remaining 4 frames with meta tags can appear in any order.  There are
  // 3 remote frames and 1 local frame.
  int local_meta_tags_frames = 0;
  int remote_meta_tags_frames = 0;

  GURL local_meta_tags_url = https_server()->GetURL("a.com", "/meta_tags.html");
  GURL remote_meta_tags_url_b =
      https_server()->GetURL("b.com", "/meta_tags.html");
  GURL remote_meta_tags_url_c =
      https_server()->GetURL("c.com", "/meta_tags.html");

  for (size_t i = 1; i < metadata->frame_metadata.size(); ++i) {
    const auto& frame_info = metadata->frame_metadata[i];
    ASSERT_EQ(frame_info->meta_tags.size(), 1u);
    EXPECT_EQ(frame_info->meta_tags[0]->name, "author");
    EXPECT_EQ(frame_info->meta_tags[0]->content, "Gary");

    if (frame_info->url == local_meta_tags_url) {
      local_meta_tags_frames++;
    } else if (frame_info->url == remote_meta_tags_url_b ||
               frame_info->url == remote_meta_tags_url_c) {
      remote_meta_tags_frames++;
    }
  }

  EXPECT_EQ(local_meta_tags_frames, 1);
  EXPECT_EQ(remote_meta_tags_frames, 3);

  observer_.reset();
}

IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       MetaTagsUpdated) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));
  CreateObserver();
  ASSERT_TRUE(callback_waiter_.Take());
  // ASSERT_TRUE(callback_called());

  // Verify initial state.
  {
    blink::mojom::PageMetadataPtr& metadata = page_metadata();
    EXPECT_EQ(metadata->frame_metadata.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->name, "author");
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->content, "Gary");
  }

  // Modify an existing tag.
  callback_waiter_.Clear();
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "document.querySelector('meta[name=\"author\"]').setAttribute('content', "
      "'Val');"));
  ASSERT_TRUE(callback_waiter_.Take());
  {
    blink::mojom::PageMetadataPtr& metadata = page_metadata();
    EXPECT_EQ(metadata->frame_metadata.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->name, "author");
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->content, "Val");
  }

  // Add a new tag.
  callback_waiter_.Clear();
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "var meta = document.createElement('meta'); meta.name = 'subject'; "
      "meta.content = 'testing'; document.head.appendChild(meta);"));
  ASSERT_TRUE(callback_waiter_.Take());
  {
    blink::mojom::PageMetadataPtr& metadata = page_metadata();
    EXPECT_EQ(metadata->frame_metadata.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 2u);
    // Order is not guaranteed.
    EXPECT_TRUE(
        (metadata->frame_metadata[0]->meta_tags[0]->name == "author" &&
         metadata->frame_metadata[0]->meta_tags[1]->name == "subject") ||
        (metadata->frame_metadata[0]->meta_tags[0]->name == "subject" &&
         metadata->frame_metadata[0]->meta_tags[1]->name == "author"));
  }

  // Remove a tag.
  callback_waiter_.Clear();
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "document.querySelector('meta[name=\"author\"]').remove();"));
  ASSERT_TRUE(callback_waiter_.Take());
  {
    blink::mojom::PageMetadataPtr& metadata = page_metadata();
    EXPECT_EQ(metadata->frame_metadata.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->name, "subject");
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->content, "testing");
  }
}

IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       MetaTagsAreRemoved) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));
  CreateObserver();
  ASSERT_TRUE(callback_waiter_.Wait());

  blink::mojom::PageMetadataPtr& metadata = page_metadata();
  EXPECT_EQ(metadata->frame_metadata.size(), 1u);
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);

  callback_waiter_.Clear();

  // Now, remove the meta tag.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "document.querySelector('meta[name=\"author\"]').remove();"));

  // The observer should be notified of the change.
  ASSERT_TRUE(callback_waiter_.Wait());

  // The metadata should now contain one frame with no meta tags.
  EXPECT_EQ(page_metadata()->frame_metadata.size(), 1u);
  EXPECT_EQ(page_metadata()->frame_metadata[0]->meta_tags.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       SubscriptionIsRemoved) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));
  CreateObserver();
  ASSERT_TRUE(callback_waiter_.Wait());

  blink::mojom::PageMetadataPtr& metadata = page_metadata();
  EXPECT_EQ(metadata->frame_metadata.size(), 1u);
  EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);

  // Now, destroy the observer.
  observer_.reset();
  callback_waiter_.Clear();

  // Now, modify the meta tag.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      "document.querySelector('meta[name=\"author\"]').setAttribute('content', "
      "'Val');"));

  ProcessPendingIPC();

  // The observer should not be notified of the change.
  EXPECT_FALSE(callback_waiter_.IsReady());
}

IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       SubscriptionIsRemovedBeforeFirstCallback) {
  CreateObserver();
  observer_.reset();
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));
  WaitForPageLoadedAndIPCs();
  EXPECT_FALSE(callback_waiter_.IsReady());
}

IN_PROC_BROWSER_TEST_F(PageContentMetadataObserverBrowserTest,
                       MetaTagsAreObservedInNavigatedIframe) {
  GURL main_url = https_server()->GetURL("/iframe.html");
  GURL iframe_url = https_server()->GetURL("/meta_tags.html");

  ASSERT_TRUE(LoadPage(main_url));
  WaitForPageLoadedAndIPCs();

  CreateObserver();
  WaitForCallback();

  // The first callback should have metadata for one or two frames (depending on
  // platform), but no meta tags.
  ASSERT_TRUE(page_metadata());
  EXPECT_TRUE(page_metadata()->frame_metadata.size() == 1u ||
              page_metadata()->frame_metadata.size() == 2u);
  for (const auto& frame_metadata : page_metadata()->frame_metadata) {
    EXPECT_TRUE(frame_metadata->meta_tags.empty());
  }
  // Any additional initial callbacks will be handled by the while-loop below.

  // Now, navigate the iframe.
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace("document.querySelector('#test_iframe').src = $1",
                         iframe_url)));

  // The observer should be notified of the meta tags in the iframe. There may
  // be multiple callbacks, so we loop until we see the metadata we expect.
  while (true) {
    ASSERT_TRUE(callback_waiter_.Wait());
    bool found_iframe_metadata = false;
    for (const auto& frame_metadata : page_metadata()->frame_metadata) {
      if (frame_metadata->url == iframe_url &&
          !frame_metadata->meta_tags.empty()) {
        found_iframe_metadata = true;
        break;
      }
    }
    if (found_iframe_metadata) {
      break;
    }
    callback_waiter_.Clear();
  }

  blink::mojom::PageMetadataPtr& metadata = page_metadata();
  ASSERT_EQ(metadata->frame_metadata.size(), 2u);

  bool found_iframe_metadata = false;
  for (const auto& frame_metadata : metadata->frame_metadata) {
    if (frame_metadata->url == iframe_url) {
      found_iframe_metadata = true;
      ASSERT_EQ(frame_metadata->meta_tags.size(), 1u);
      EXPECT_EQ(frame_metadata->meta_tags[0]->name, "author");
      EXPECT_EQ(frame_metadata->meta_tags[0]->content, "Gary");
    }
  }
  EXPECT_TRUE(found_iframe_metadata);
  observer_.reset();
}

}  // namespace

}  // namespace optimization_guide
