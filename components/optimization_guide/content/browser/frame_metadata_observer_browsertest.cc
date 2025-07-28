// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content_metadata.mojom.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom.h"
#include "ui/display/display_switches.h"

// TODO(gklassen): move this test to chrome/browser/content_extraction/

namespace optimization_guide {

namespace {

base::FilePath GetTestDataDir() {
  return base::FilePath{
      FILE_PATH_LITERAL("components/test/data/optimization_guide")};
}

class FrameMetadataObserverBrowserTest
    : public content::ContentBrowserTest,
      public blink::mojom::PaidContentMetadataObserver,
      public blink::mojom::MetaTagsObserver {
 public:
  FrameMetadataObserverBrowserTest() = default;

  FrameMetadataObserverBrowserTest(const FrameMetadataObserverBrowserTest&) =
      delete;
  FrameMetadataObserverBrowserTest& operator=(
      const FrameMetadataObserverBrowserTest&) = delete;

  ~FrameMetadataObserverBrowserTest() override = default;

  content::WebContents* web_contents() { return shell()->web_contents(); }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();  // Call parent setup
                                                       // first

    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());

    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);

    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
  }

  bool LoadPage(GURL url) {
    paid_content_callback_waiter_.Clear();
    metadata_callback_waiter_.Clear();
    return content::NavigateToURL(web_contents(), url);
  }

  bool WaitForRenderFrameReady() {
    return content::WaitForRenderFrameReady(
        web_contents()->GetPrimaryMainFrame());
  }

  void BindRegistry() {
    if (frame_metadata_observer_registry_.is_bound()) {
      return;
    }
    web_contents()->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
        frame_metadata_observer_registry_.BindNewPipeAndPassReceiver());
  }

  void AddPaidContentObserver() {
    BindRegistry();
    mojo::PendingRemote<blink::mojom::PaidContentMetadataObserver> remote;
    frame_metadata_observer_receiver_.Bind(
        remote.InitWithNewPipeAndPassReceiver());

    frame_metadata_observer_registry_->AddPaidContentMetadataObserver(
        std::move(remote));
  }

  // Invoked when the frame metadata changes.
  void OnPaidContentMetadataChanged(bool has_paid_content) override {
    paid_content_callback_waiter_.SetValue(has_paid_content);
  }

  void WaitForPaidContentChanged() {
    ASSERT_TRUE(paid_content_callback_waiter_.Wait())
        << "Timed out waiting for OnPaidContentMetadataChanged callback";
  }

  bool hasPaidContent() { return paid_content_callback_waiter_.Get(); }

  void AddMetaTagsObserver(const std::vector<std::string>& names) {
    BindRegistry();
    mojo::PendingRemote<blink::mojom::MetaTagsObserver> remote;
    meta_tags_observer_receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());

    frame_metadata_observer_registry_->AddMetaTagsObserver(names,
                                                           std::move(remote));
  }

  // Invoked when the frame metadata changes.
  void OnMetaTagsChanged(blink::mojom::PageMetadataPtr page_metadata) override {
    metadata_callback_waiter_.SetValue(true);
    page_metadata_ = std::move(page_metadata);
  }

  void WaitForMetaTagsChanged() {
    ASSERT_TRUE(metadata_callback_waiter_.Wait())
        << "Timed out waiting for OnMetaTagsChanged callback";
  }

  bool was_meta_tags_changed_called() {
    return metadata_callback_waiter_.Get();
  }
  blink::mojom::PageMetadataPtr& page_metadata() { return page_metadata_; }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void VerifyAuthorMetaTag() {
    EXPECT_TRUE(was_meta_tags_changed_called());

    blink::mojom::PageMetadataPtr& metadata = page_metadata();
    EXPECT_EQ(metadata->frame_metadata.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags.size(), 1u);
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->name, "author");
    EXPECT_EQ(metadata->frame_metadata[0]->meta_tags[0]->content, "Gary");
  }

 protected:
  // TestFuture that will be signaled when the callback runs.
  base::test::TestFuture<bool> paid_content_callback_waiter_;
  base::test::TestFuture<bool> metadata_callback_waiter_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  mojo::Remote<blink::mojom::FrameMetadataObserverRegistry>
      frame_metadata_observer_registry_;
  mojo::Receiver<blink::mojom::PaidContentMetadataObserver>
      frame_metadata_observer_receiver_{this};
  mojo::Receiver<blink::mojom::MetaTagsObserver> meta_tags_observer_receiver_{
      this};

  blink::mojom::PageMetadataPtr page_metadata_;
};

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, PaidContent) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/paid_content.html")));

  AddPaidContentObserver();
  WaitForPaidContentChanged();

  EXPECT_TRUE(hasPaidContent());
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, NoPaidContent) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/simple.html")));

  AddPaidContentObserver();
  WaitForPaidContentChanged();

  EXPECT_FALSE(hasPaidContent());
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, LateObserver) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/paid_content.html")));

  // Wait for the page to load before adding the observer.
  ASSERT_TRUE(WaitForRenderFrameReady());

  AddPaidContentObserver();
  WaitForPaidContentChanged();

  EXPECT_TRUE(hasPaidContent());
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, MetaTags) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));

  const std::vector<std::string> names = {"author", "subject"};

  AddMetaTagsObserver(names);
  WaitForMetaTagsChanged();

  VerifyAuthorMetaTag();
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, MetaTagsLateObserver) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));

  // Wait for the page to load before adding the observer.
  ASSERT_TRUE(WaitForRenderFrameReady());

  const std::vector<std::string> names = {"author", "subject"};
  AddMetaTagsObserver(names);
  WaitForMetaTagsChanged();

  VerifyAuthorMetaTag();
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, MetaTagsNameMismatch) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/meta_tags.html")));

  const std::vector<std::string> names = {"subject", "category"};

  AddMetaTagsObserver(names);
  WaitForMetaTagsChanged();

  EXPECT_TRUE(was_meta_tags_changed_called());
  blink::mojom::PageMetadataPtr& metadata = page_metadata();
  EXPECT_TRUE(metadata->frame_metadata.empty());
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, NoMetaTags) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/simple.html")));

  const std::vector<std::string> names = {"author", "subject"};
  AddMetaTagsObserver(names);

  WaitForMetaTagsChanged();

  EXPECT_TRUE(was_meta_tags_changed_called());
  blink::mojom::PageMetadataPtr& metadata = page_metadata();
  EXPECT_TRUE(metadata->frame_metadata.empty());
}

}  // namespace

}  // namespace optimization_guide
