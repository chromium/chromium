// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/secure_embed/browser/secure_embed_host.h"
#include "components/secure_embed/common/secure_embed.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace secure_embed {

namespace {

constexpr char kTestUrl[] = "/secure_embed/embed_tag.html";
constexpr char kMultipleEmbedsUrl[] = "/secure_embed/multiple_embeds.html";

constexpr size_t kSingleEmbedCount = 1;
constexpr size_t kMultipleEmbedCount = 3;

class MockSecureEmbedHost;

// Helper class for tracking MockSecureEmbedHost instances.
class SecureEmbedHostTracker {
 public:
  SecureEmbedHostTracker() = default;

  SecureEmbedHostTracker(const SecureEmbedHostTracker&) = delete;
  SecureEmbedHostTracker& operator=(const SecureEmbedHostTracker&) = delete;

  ~SecureEmbedHostTracker() = default;

  void AddMockHost(MockSecureEmbedHost* host) {
    mock_hosts_.push_back(host);
    if (host_added_callback_) {
      std::move(host_added_callback_).Run();
    }
  }

  void RemoveMockHost(MockSecureEmbedHost* host) {
    auto it = std::find(mock_hosts_.begin(), mock_hosts_.end(), host);
    CHECK(it != mock_hosts_.end());
    mock_hosts_.erase(it);
  }

  void SetHostAddedCallback(base::OnceClosure callback) {
    DCHECK(!host_added_callback_);
    host_added_callback_ = std::move(callback);
  }

  const std::vector<MockSecureEmbedHost*>& mock_hosts() const {
    return mock_hosts_;
  }

  MockSecureEmbedHost* GetMockHost(size_t index) const {
    if (index < mock_hosts_.size()) {
      return mock_hosts_[index];
    }
    return nullptr;
  }

  size_t GetMockHostCount() const { return mock_hosts_.size(); }

 private:
  std::vector<MockSecureEmbedHost*> mock_hosts_;
  base::OnceClosure host_added_callback_;
};

class MockSecureEmbedHost : public mojom::SecureEmbedHost {
 public:
  explicit MockSecureEmbedHost(SecureEmbedHostTracker* tracker)
      : tracker_(tracker) {
    tracker_->AddMockHost(this);
  }

  ~MockSecureEmbedHost() override {
    // Manually calling the disconnect callback in the d'tor. This is
    // effectively the same as calling it from connection_error_handler since
    // this object is kept alive via SelfOwnedAssociatedReceiver which detects
    // disconnection and deletes this object.
    if (disconnect_callback_) {
      std::move(disconnect_callback_).Run();
    }
    tracker_->RemoveMockHost(this);
  }

  // mojom::SecureEmbedHost implementation
  void SetSecureEmbed(
      mojo::PendingAssociatedRemote<mojom::SecureEmbed> secure_embed) override {
    secure_embed_.Bind(std::move(secure_embed));
    secure_embed_.set_disconnect_handler(
        base::BindOnce(&MockSecureEmbedHost::OnSecureEmbedDisconnected,
                       base::Unretained(this)));
  }

  void Attach(int64_t content_id) override {
    CHECK(secure_embed_);
    attach_call_count_++;
    last_content_id_ = content_id;

    if (attach_callback_) {
      std::move(attach_callback_).Run(content_id);
    }
  }

  void OnSecureEmbedDisconnected() { secure_embed_.reset(); }

  void SetAttachCallback(base::OnceCallback<void(int64_t)> callback) {
    attach_callback_ = std::move(callback);
  }

  void SetDisconnectCallback(base::OnceClosure callback) {
    disconnect_callback_ = std::move(callback);
  }

  int attach_call_count() const { return attach_call_count_; }
  int64_t last_content_id() const { return last_content_id_; }

 private:
  raw_ptr<SecureEmbedHostTracker> tracker_;
  int attach_call_count_ = 0;
  int64_t last_content_id_ = -1;
  base::OnceCallback<void(int64_t)> attach_callback_;
  base::OnceClosure disconnect_callback_;
  mojo::AssociatedRemote<mojom::SecureEmbed> secure_embed_;
};

class SecureEmbedTestContentBrowserClient
    : public content::ContentBrowserTestContentBrowserClient {
 public:
  explicit SecureEmbedTestContentBrowserClient(SecureEmbedHostTracker* tracker)
      : tracker_(tracker) {}
  ~SecureEmbedTestContentBrowserClient() override = default;

  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override {
    associated_registry.AddInterface<mojom::SecureEmbedHost>(
        base::BindRepeating(
            [](SecureEmbedHostTracker* tracker,
               content::RenderFrameHost* render_frame_host,
               mojo::PendingAssociatedReceiver<mojom::SecureEmbedHost>
                   receiver) {
              mojo::MakeSelfOwnedAssociatedReceiver(
                  std::make_unique<MockSecureEmbedHost>(tracker),
                  std::move(receiver));
            },
            base::Unretained(tracker_), &render_frame_host));
  }

 private:
  raw_ptr<SecureEmbedHostTracker> tracker_;
};

}  // namespace

class SecureEmbedRendererTest : public content::ContentBrowserTest {
 public:
  SecureEmbedRendererTest() = default;

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
        std::make_unique<SecureEmbedTestContentBrowserClient>(&tracker_);
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

  bool WaitForAttachCall(MockSecureEmbedHost* host) {
    if (host->attach_call_count() > 0) {
      return true;
    }

    base::RunLoop run_loop;
    host->SetAttachCallback(base::BindLambdaForTesting(
        [&](int64_t content_id) { run_loop.Quit(); }));
    run_loop.Run();
    return true;
  }

  void WaitForHostAdded(size_t expected_count) {
    if (GetMockHostCount() >= expected_count) {
      return;
    }
    base::RunLoop run_loop;
    tracker_.SetHostAddedCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  MockSecureEmbedHost* GetMockHost(size_t index) {
    return tracker_.GetMockHost(index);
  }

  size_t GetMockHostCount() const { return tracker_.GetMockHostCount(); }

 protected:
  SecureEmbedHostTracker tracker_;
  std::unique_ptr<SecureEmbedTestContentBrowserClient> test_browser_client_;
};

IN_PROC_BROWSER_TEST_F(SecureEmbedRendererTest, EmbedTagCreatesPlugin) {
  NavigateToTestUrl(kTestUrl);

  EXPECT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSecureEmbedHost* host = GetMockHost(0);
  ASSERT_NE(nullptr, host);

  ASSERT_TRUE(WaitForAttachCall(host));
  EXPECT_EQ(1, host->attach_call_count());

  EXPECT_EQ(1, host->last_content_id());
}

IN_PROC_BROWSER_TEST_F(SecureEmbedRendererTest, MultipleEmbedTags) {
  NavigateToTestUrl(kMultipleEmbedsUrl);

  ASSERT_EQ(kMultipleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kMultipleEmbedCount, GetMockHostCount());

  // Verify each host has an independent Mojo connection and receives Attach().
  // Collect all content_ids to verify we see 1, 2, and 3 (order is not
  // deterministic).
  std::set<int64_t> content_ids;
  for (size_t i = 0; i < kMultipleEmbedCount; i++) {
    MockSecureEmbedHost* host = GetMockHost(i);
    ASSERT_NE(nullptr, host);
    ASSERT_TRUE(WaitForAttachCall(host));
    ASSERT_EQ(1, host->attach_call_count());
    content_ids.insert(host->last_content_id());
  }

  EXPECT_EQ(std::set<int64_t>({1, 2, 3}), content_ids);
}

IN_PROC_BROWSER_TEST_F(SecureEmbedRendererTest, PluginDestruction) {
  // Verifies that navigating away from a page with a plugin properly destroys
  // the SecureEmbedWebPlugin and SecureEmbedHost, and that the Mojo connection
  // is cleanly closed.
  NavigateToTestUrl(kTestUrl);
  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());
  MockSecureEmbedHost* first_host = GetMockHost(0);
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
    MockSecureEmbedHost* host = GetMockHost(i);
    ASSERT_NE(nullptr, host);
    ASSERT_TRUE(WaitForAttachCall(host));
  }

  std::vector<std::unique_ptr<base::RunLoop>> disconnect_loops;
  std::vector<bool> hosts_disconnected(kMultipleEmbedCount, false);
  for (size_t i = 0; i < kMultipleEmbedCount; i++) {
    MockSecureEmbedHost* host = GetMockHost(i);
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

IN_PROC_BROWSER_TEST_F(SecureEmbedRendererTest, RemoveEmbedFromDOM) {
  // Verifies that removing an embed element from the DOM destroys the
  // SecureEmbedHost connection.
  NavigateToTestUrl(kTestUrl);

  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSecureEmbedHost* host = GetMockHost(0);
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

IN_PROC_BROWSER_TEST_F(SecureEmbedRendererTest, RemoveAndReinsertEmbed) {
  // Verifies that removing an embed element from the DOM (while keeping it
  // alive in JS) destroys the SecureEmbedHost connection, and re-inserting
  // it creates a new SecureEmbedHost connection.
  NavigateToTestUrl(kTestUrl);

  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSecureEmbedHost* first_host = GetMockHost(0);
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

  MockSecureEmbedHost* second_host = GetMockHost(0);
  ASSERT_NE(nullptr, second_host);

  // Wait for the new host to receive the Attach call
  ASSERT_TRUE(WaitForAttachCall(second_host));
  EXPECT_EQ(1, second_host->attach_call_count());
}

IN_PROC_BROWSER_TEST_F(SecureEmbedRendererTest,
                       ChangeDataContentIdDoesNotRecreateHost) {
  // Verifies that changing the data-content-id attribute does not destroy
  // and recreate the SecureEmbedHost connection.
  constexpr size_t kFinalEmbedCount = 2;

  NavigateToTestUrl(kTestUrl);

  ASSERT_EQ(kSingleEmbedCount, CountEmbedElementsInPage());
  ASSERT_EQ(kSingleEmbedCount, GetMockHostCount());

  MockSecureEmbedHost* host = GetMockHost(0);
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

  MockSecureEmbedHost* second_host = GetMockHost(1);
  ASSERT_NE(nullptr, second_host);
  ASSERT_TRUE(WaitForAttachCall(second_host));
  EXPECT_EQ(1, second_host->attach_call_count());
  EXPECT_EQ(10, second_host->last_content_id());

  // Verify the original host was not disconnected or recreated
  EXPECT_FALSE(host_disconnected);
  EXPECT_EQ(host, GetMockHost(0));
  EXPECT_EQ(1, host->attach_call_count());
  EXPECT_EQ(1, host->last_content_id());
}

}  // namespace secure_embed
