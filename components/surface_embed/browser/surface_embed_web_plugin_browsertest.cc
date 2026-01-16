// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "components/surface_embed/browser/surface_embed_host.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace surface_embed {

namespace {

constexpr char kTestUrl[] = "/surface_embed/embed_tag.html";
constexpr char kMultipleEmbedsUrl[] = "/surface_embed/multiple_embeds.html";

constexpr size_t kSingleEmbedCount = 1;
constexpr size_t kMultipleEmbedCount = 3;

class MockSurfaceEmbedHost;

// Helper class for tracking MockSurfaceEmbedHost instances.
class SurfaceEmbedHostTracker {
 public:
  SurfaceEmbedHostTracker() = default;

  SurfaceEmbedHostTracker(const SurfaceEmbedHostTracker&) = delete;
  SurfaceEmbedHostTracker& operator=(const SurfaceEmbedHostTracker&) = delete;

  ~SurfaceEmbedHostTracker() = default;

  void AddMockHost(MockSurfaceEmbedHost* host) {
    mock_hosts_.push_back(host);
    if (host_added_callback_) {
      std::move(host_added_callback_).Run();
    }
  }

  void RemoveMockHost(MockSurfaceEmbedHost* host) {
    auto it = std::find(mock_hosts_.begin(), mock_hosts_.end(), host);
    CHECK(it != mock_hosts_.end());
    mock_hosts_.erase(it);
  }

  void SetHostAddedCallback(base::OnceClosure callback) {
    DCHECK(!host_added_callback_);
    host_added_callback_ = std::move(callback);
  }

  const std::vector<MockSurfaceEmbedHost*>& mock_hosts() const {
    return mock_hosts_;
  }

  MockSurfaceEmbedHost* GetMockHost(size_t index) const {
    if (index < mock_hosts_.size()) {
      return mock_hosts_[index];
    }
    return nullptr;
  }

  size_t GetMockHostCount() const { return mock_hosts_.size(); }

 private:
  std::vector<MockSurfaceEmbedHost*> mock_hosts_;
  base::OnceClosure host_added_callback_;
};

class MockSurfaceEmbedHost : public mojom::SurfaceEmbedHost {
 public:
  explicit MockSurfaceEmbedHost(SurfaceEmbedHostTracker* tracker)
      : tracker_(tracker) {
    tracker_->AddMockHost(this);
  }

  ~MockSurfaceEmbedHost() override {
    // Manually calling the disconnect callback in the d'tor. This is
    // effectively the same as calling it from connection_error_handler since
    // this object is kept alive via SelfOwnedAssociatedReceiver which detects
    // disconnection and deletes this object.
    if (disconnect_callback_) {
      std::move(disconnect_callback_).Run();
    }
    tracker_->RemoveMockHost(this);
  }

  // mojom::SurfaceEmbedHost implementation
  void SetSurfaceEmbed(mojo::PendingAssociatedRemote<mojom::SurfaceEmbed>
                           surface_embed) override {
    surface_embed_.Bind(std::move(surface_embed));
    surface_embed_.set_disconnect_handler(
        base::BindOnce(&MockSurfaceEmbedHost::OnSurfaceEmbedDisconnected,
                       base::Unretained(this)));
  }

  void AttachConnector(int64_t content_id) override {
    CHECK(surface_embed_);
    attach_call_count_++;
    last_content_id_ = content_id;
  }

  void DetachConnector() override {
    detach_call_count_++;
    // TODO(surface-embed): Create a constant for invalid content ID.
    last_content_id_ = 0;
  }

  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool is_visible) override {}

  void SetFocus(bool focused, blink::mojom::FocusType focus_type) override {}

  void OnSurfaceEmbedDisconnected() { surface_embed_.reset(); }

  void SetDisconnectCallback(base::OnceClosure callback) {
    disconnect_callback_ = std::move(callback);
  }

  int attach_call_count() const { return attach_call_count_; }
  int detach_call_count() const { return detach_call_count_; }
  int64_t last_content_id() const { return last_content_id_; }

 private:
  raw_ptr<SurfaceEmbedHostTracker> tracker_;
  int attach_call_count_ = 0;
  int detach_call_count_ = 0;
  int64_t last_content_id_ = -1;
  base::OnceClosure disconnect_callback_;
  mojo::AssociatedRemote<mojom::SurfaceEmbed> surface_embed_;
};

class SurfaceEmbedTestContentBrowserClient
    : public content::ContentBrowserTestContentBrowserClient {
 public:
  explicit SurfaceEmbedTestContentBrowserClient(
      SurfaceEmbedHostTracker* tracker)
      : tracker_(tracker) {}
  ~SurfaceEmbedTestContentBrowserClient() override = default;

  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override {
    associated_registry.AddInterface<mojom::SurfaceEmbedHost>(
        base::BindRepeating(
            [](SurfaceEmbedHostTracker* tracker,
               content::RenderFrameHost* render_frame_host,
               mojo::PendingAssociatedReceiver<mojom::SurfaceEmbedHost>
                   receiver) {
              mojo::MakeSelfOwnedAssociatedReceiver(
                  std::make_unique<MockSurfaceEmbedHost>(tracker),
                  std::move(receiver));
            },
            base::Unretained(tracker_), &render_frame_host));
  }

 private:
  raw_ptr<SurfaceEmbedHostTracker> tracker_;
};

}  // namespace

class SurfaceEmbedRendererTest : public content::ContentBrowserTest {
 public:
  SurfaceEmbedRendererTest() = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    content::ContentBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    test_browser_client_ =
        std::make_unique<SurfaceEmbedTestContentBrowserClient>(&tracker_);
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  void NavigateToTestUrl(const char* url) {
    const GURL test_url = embedded_test_server()->GetURL(url);
    ASSERT_TRUE(NavigateToURL(web_contents(), test_url));
  }

  int CountEmbedElementsInPage() {
    return content::EvalJs(web_contents(), "document.embeds.length")
        .ExtractInt();
  }

  bool WaitForAttachCall(MockSurfaceEmbedHost* host) {
    return base::test::RunUntil(
        [host]() { return host->attach_call_count() > 0; });
  }

  bool WaitForDetachCall(MockSurfaceEmbedHost* host) {
    return base::test::RunUntil(
        [host]() { return host->detach_call_count() > 0; });
  }

  void WaitForHostAdded(size_t expected_count) {
    if (GetMockHostCount() >= expected_count) {
      return;
    }
    base::RunLoop run_loop;
    tracker_.SetHostAddedCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  MockSurfaceEmbedHost* GetMockHost(size_t index) {
    return tracker_.GetMockHost(index);
  }

  size_t GetMockHostCount() const { return tracker_.GetMockHostCount(); }

 protected:
  SurfaceEmbedHostTracker tracker_;
  std::unique_ptr<SurfaceEmbedTestContentBrowserClient> test_browser_client_;
};

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest, EmbedTagCreatesPlugin) {
  NavigateToTestUrl(kTestUrl);

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* host = GetMockHost(0);
  ASSERT_NE(nullptr, host);

  ASSERT_TRUE(WaitForAttachCall(host));
  EXPECT_EQ(1, host->attach_call_count());

  EXPECT_EQ(1, host->last_content_id());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest, MultipleEmbedTags) {
  NavigateToTestUrl(kMultipleEmbedsUrl);

  ASSERT_EQ(kMultipleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kMultipleEmbedCount, GetMockHostCount());

  // Verify each host has an independent Mojo connection and receives a call to
  // AttachConnector(). Collect all content_ids to verify we see 1, 2, and 3
  // (order is not deterministic).
  std::set<int64_t> content_ids;
  for (size_t i = 0; i < kMultipleEmbedCount; i++) {
    MockSurfaceEmbedHost* host = GetMockHost(i);
    ASSERT_NE(nullptr, host);
    ASSERT_TRUE(WaitForAttachCall(host));
    ASSERT_EQ(1, host->attach_call_count());
    content_ids.insert(host->last_content_id());
  }

  EXPECT_EQ(std::set<int64_t>({1, 2, 3}), content_ids);
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest, PluginDestruction) {
  // Verifies that navigating away from a page with a plugin properly destroys
  // the SurfaceEmbedWebPlugin and SurfaceEmbedHost, and that the Mojo
  // connection is cleanly closed.
  NavigateToTestUrl(kTestUrl);
  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());
  MockSurfaceEmbedHost* first_host = GetMockHost(0);
  ASSERT_NE(nullptr, first_host);

  ASSERT_TRUE(WaitForAttachCall(first_host));

  base::RunLoop disconnect_loop;
  bool first_host_disconnected = false;
  first_host->SetDisconnectCallback(base::BindLambdaForTesting([&]() {
    first_host_disconnected = true;
    first_host = nullptr;
    disconnect_loop.Quit();
  }));

  NavigateToTestUrl(kMultipleEmbedsUrl);
  ASSERT_EQ(kMultipleEmbedCount, CountEmbedElementsInPage());

  // Navigation should have disconnected the first host
  disconnect_loop.Run();
  EXPECT_TRUE(first_host_disconnected);

  ASSERT_EQ(kMultipleEmbedCount, GetMockHostCount());

  for (size_t i = 0; i < kMultipleEmbedCount; i++) {
    MockSurfaceEmbedHost* host = GetMockHost(i);
    ASSERT_NE(nullptr, host);
    ASSERT_TRUE(WaitForAttachCall(host));
  }

  std::vector<std::unique_ptr<base::RunLoop>> disconnect_loops;
  std::vector<bool> hosts_disconnected(kMultipleEmbedCount, false);
  for (size_t i = 0; i < kMultipleEmbedCount; i++) {
    MockSurfaceEmbedHost* host = GetMockHost(i);
    auto loop = std::make_unique<base::RunLoop>();
    host->SetDisconnectCallback(base::BindLambdaForTesting(
        [&hosts_disconnected, i, loop_ptr = loop.get()]() {
          hosts_disconnected[i] = true;
          loop_ptr->Quit();
        }));
    disconnect_loops.push_back(std::move(loop));
  }

  ASSERT_TRUE(NavigateToURL(web_contents(), GURL("about:blank")));
  EXPECT_EQ(0, CountEmbedElementsInPage());

  for (size_t i = 0; i < disconnect_loops.size(); i++) {
    disconnect_loops[i]->Run();
    EXPECT_TRUE(hosts_disconnected[i]);
  }

  ASSERT_EQ(0u, GetMockHostCount());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest, RemoveEmbedFromDOM) {
  // Verifies that removing an embed element from the DOM destroys the
  // SurfaceEmbedHost connection.
  NavigateToTestUrl(kTestUrl);

  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* host = GetMockHost(0);
  ASSERT_NE(nullptr, host);
  ASSERT_TRUE(WaitForAttachCall(host));

  base::RunLoop disconnect_loop;
  bool host_disconnected = false;
  host->SetDisconnectCallback(base::BindLambdaForTesting([&]() {
    host_disconnected = true;
    host = nullptr;
    disconnect_loop.Quit();
  }));

  // Remove the embed element from the DOM using JavaScript
  ASSERT_TRUE(content::ExecJs(web_contents(), "document.embeds[0].remove();"));

  // Wait for the disconnect callback to be triggered
  disconnect_loop.Run();
  EXPECT_TRUE(host_disconnected);

  // Verify there are now no embed elements in the page
  EXPECT_EQ(0, CountEmbedElementsInPage());

  // Verify no hosts remain
  EXPECT_EQ(0u, GetMockHostCount());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest, RemoveAndReinsertEmbed) {
  // Verifies that removing an embed element from the DOM (while keeping it
  // alive in JS) destroys the SurfaceEmbedHost connection, and re-inserting
  // it creates a new SurfaceEmbedHost connection.
  NavigateToTestUrl(kTestUrl);

  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* first_host = GetMockHost(0);
  ASSERT_NE(nullptr, first_host);
  ASSERT_TRUE(WaitForAttachCall(first_host));

  base::RunLoop disconnect_loop;
  bool first_host_disconnected = false;
  first_host->SetDisconnectCallback(base::BindLambdaForTesting([&]() {
    first_host_disconnected = true;
    first_host = nullptr;
    disconnect_loop.Quit();
  }));

  // Remove the embed element from the DOM but keep a JS reference to it
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "window.savedEmbed = document.embeds[0]; "
                              "window.savedEmbed.remove();"));

  disconnect_loop.Run();
  EXPECT_TRUE(first_host_disconnected);
  EXPECT_EQ(0, CountEmbedElementsInPage());
  EXPECT_EQ(0u, GetMockHostCount());

  // Re-insert the embed element back into the DOM
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.body.appendChild(window.savedEmbed);"));

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());

  WaitForHostAdded(kSingleEmbedCount);
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* second_host = GetMockHost(0);
  ASSERT_NE(nullptr, second_host);

  // Wait for the new host to receive the Attach call
  ASSERT_TRUE(WaitForAttachCall(second_host));
  EXPECT_EQ(1, second_host->attach_call_count());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest,
                       ChangeDataContentIdDoesNotRecreateHost) {
  // Verifies that changing the data-content-id attribute does not destroy
  // and recreate the SurfaceEmbedHost connection.
  constexpr size_t kFinalEmbedCount = 2;

  NavigateToTestUrl(kTestUrl);

  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* host = GetMockHost(0);
  ASSERT_NE(nullptr, host);
  ASSERT_TRUE(WaitForAttachCall(host));
  EXPECT_EQ(1, host->last_content_id());

  // Set up disconnect callback to detect if host is destroyed
  bool host_disconnected = false;
  host->SetDisconnectCallback(
      base::BindLambdaForTesting([&]() { host_disconnected = true; }));

  // Change the data-content-id attribute
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.embeds[0].setAttribute('data-content-id', '5');"));

  // Add a new embed element and wait for it to attach. This ensures that
  // the renderer and browser have fully processed the attribute change above.
  ASSERT_TRUE(
      content::ExecJs(web_contents(),
                      "const newEmbed = document.createElement('embed');"
                      "newEmbed.setAttribute('type', "
                      "'application/x-google-chrome-secure-embed');"
                      "newEmbed.setAttribute('data-content-id', '10');"
                      "document.body.appendChild(newEmbed);"));

  EXPECT_EQ(kFinalEmbedCount, CountEmbedElementsInPage());
  WaitForHostAdded(kFinalEmbedCount);
  ASSERT_EQ(kFinalEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* second_host = GetMockHost(1);
  ASSERT_NE(nullptr, second_host);
  ASSERT_TRUE(WaitForAttachCall(second_host));
  EXPECT_EQ(1, second_host->attach_call_count());
  EXPECT_EQ(10, second_host->last_content_id());

  // Verify the original host was not disconnected or recreated
  EXPECT_FALSE(host_disconnected);
  EXPECT_EQ(host, GetMockHost(0));
  // But we do expect it to have received a second AttachConnector() call with
  // the new content ID.
  EXPECT_EQ(2, host->attach_call_count());
  EXPECT_EQ(5, host->last_content_id());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest, Detach) {
  NavigateToTestUrl(kTestUrl);

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* host = GetMockHost(0);
  ASSERT_NE(nullptr, host);

  ASSERT_TRUE(WaitForAttachCall(host));
  EXPECT_EQ(1, host->attach_call_count());
  EXPECT_EQ(1, host->last_content_id());

  // Change the data-content-id attribute to 0 to trigger a detach.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.embeds[0].setAttribute('data-content-id', '0');"));

  ASSERT_TRUE(WaitForDetachCall(host));
  EXPECT_EQ(1, host->detach_call_count());
  EXPECT_EQ(0, host->last_content_id());
}

IN_PROC_BROWSER_TEST_F(SurfaceEmbedRendererTest,
                       UpdateDataAttributeWithInvalidStrings) {
  NavigateToTestUrl(kTestUrl);
  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSurfaceEmbedHost* host = GetMockHost(0);
  ASSERT_NE(nullptr, host);

  ASSERT_TRUE(WaitForAttachCall(host));
  EXPECT_EQ(1, host->attach_call_count());
  EXPECT_EQ(1, host->last_content_id());

  // Test a variety of invalid data-content-id attribute values to ensure that
  // they don't lead to calls to attach on the host.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.embeds[0].setAttribute('data-content-id', 'invalid');"));
  EXPECT_EQ(1, host->attach_call_count());
  EXPECT_EQ(0, host->detach_call_count());

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.embeds[0].setAttribute('data-content-id', '123abc');"));
  EXPECT_EQ(1, host->attach_call_count());
  EXPECT_EQ(1, host->last_content_id());

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.embeds[0].setAttribute('data-content-id', '');"));
  EXPECT_EQ(1, host->attach_call_count());
  EXPECT_EQ(0, host->detach_call_count());

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.embeds[0].setAttribute('data-content-id', '5');"));
  ASSERT_TRUE(WaitForAttachCall(host));
  EXPECT_EQ(2, host->attach_call_count());
  EXPECT_EQ(5, host->last_content_id());
}

}  // namespace surface_embed
