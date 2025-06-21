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
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom.h"
#include "ui/display/display_switches.h"

// TODO(gklassen): move this test to chrome/browser/content_extraction/

namespace optimization_guide {

namespace {

// Allow 1px differences from rounding.
#define EXPECT_ALMOST_EQ(a, b) EXPECT_LE(abs(a - b), 1);

base::FilePath GetTestDataDir() {
  return base::FilePath{
      FILE_PATH_LITERAL("components/test/data/optimization_guide")};
}

class FrameMetadataObserverBrowserTest
    : public content::ContentBrowserTest,
      public blink::mojom::FrameMetadataObserver {
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
    callback_waiter_.Clear();
    return content::NavigateToURL(web_contents(), url);
  }

  bool WaitForRenderFrameReady() {
    return content::WaitForRenderFrameReady(
        web_contents()->GetPrimaryMainFrame());
  }

  void AddObserver() {
    auto* rfh = web_contents()->GetPrimaryMainFrame();
    rfh->GetRemoteInterfaces()->GetInterface(
        frame_metadata_observer_registry_.BindNewPipeAndPassReceiver());

    mojo::PendingRemote<blink::mojom::FrameMetadataObserver> remote;
    frame_metadata_observer_receiver_.Bind(
        remote.InitWithNewPipeAndPassReceiver());

    frame_metadata_observer_registry_->AddObserver(std::move(remote));
  }

  // Invoked when the frame metadata changes.
  void OnPaidContentMetadataChanged(bool has_paid_content) override {
    callback_waiter_.SetValue(has_paid_content);
  }

  void WaitForCallback() {
    ASSERT_TRUE(callback_waiter_.Wait())
        << "Timed out waiting for OnPaidContentMetadataChanged callback";
  }

  bool hasPaidContent() { return callback_waiter_.Get(); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 protected:
  // TestFuture that will be signaled when the callback runs.
  base::test::TestFuture<bool> callback_waiter_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  mojo::Remote<blink::mojom::FrameMetadataObserverRegistry>
      frame_metadata_observer_registry_;
  mojo::Receiver<blink::mojom::FrameMetadataObserver>
      frame_metadata_observer_receiver_{this};
};

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, PaidContent) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/paid_content.html")));

  AddObserver();
  WaitForCallback();

  EXPECT_TRUE(hasPaidContent());
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, NoPaidContent) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/simple.html")));

  AddObserver();
  WaitForCallback();

  EXPECT_FALSE(hasPaidContent());
}

IN_PROC_BROWSER_TEST_F(FrameMetadataObserverBrowserTest, LateObserver) {
  ASSERT_TRUE(LoadPage(https_server()->GetURL("/paid_content.html")));

  // Wait for the page to load before adding the observer.
  ASSERT_TRUE(WaitForRenderFrameReady());

  AddObserver();
  WaitForCallback();

  EXPECT_TRUE(hasPaidContent());
}

}  // namespace

}  // namespace optimization_guide
