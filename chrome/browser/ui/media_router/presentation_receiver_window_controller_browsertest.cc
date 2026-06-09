// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/presentation_receiver_window_controller.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/media_router/browser/presentation/local_presentation_manager.h"
#include "components/media_router/browser/presentation/local_presentation_manager_factory.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/script_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;

namespace {

constexpr char kPresentationId[] = "test_id";
const base::FilePath::StringViewType kResourcePath =
    FILE_PATH_LITERAL("media/router/");

base::RepeatingCallback<void(const std::string&)> GetNoopTitleChangeCallback() {
  return base::BindRepeating([](const std::string& title) {});
}

base::FilePath GetResourceFile(base::FilePath::StringViewType relative_path) {
  base::FilePath base_dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &base_dir)) {
    return base::FilePath();
  }
  base::FilePath full_path =
      base_dir.Append(kResourcePath).Append(relative_path);
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    if (!PathExists(full_path)) {
      return base::FilePath();
    }
  }
  return full_path;
}

// This class imitates a presentation controller page from a messaging
// standpoint.  It is registered as a controller connection for the appropriate
// presentation ID with the LocalPresentationManager to facilitate a
// presentation API communication test with the receiver window.
class FakeControllerConnection final
    : public blink::mojom::PresentationConnection {
 public:
  FakeControllerConnection() = default;

  FakeControllerConnection(const FakeControllerConnection&) = delete;
  FakeControllerConnection& operator=(const FakeControllerConnection&) = delete;

  void SendTextMessage(const std::string& message) {
    ASSERT_TRUE(receiver_connection_remote_.is_bound());
    receiver_connection_remote_->OnMessage(
        blink::mojom::PresentationConnectionMessage::NewMessage(message));
  }

  // blink::mojom::PresentationConnection implementation
  MOCK_METHOD(void,
              OnMessage,
              (blink::mojom::PresentationConnectionMessagePtr message));
  void DidChangeState(
      blink::mojom::PresentationConnectionState state) override {}
  void DidClose(
      blink::mojom::PresentationConnectionCloseReason reason) override {}

  mojo::PendingReceiver<blink::mojom::PresentationConnection>
  MakeConnectionRequest() {
    return receiver_connection_remote_.BindNewPipeAndPassReceiver();
  }
  mojo::PendingRemote<blink::mojom::PresentationConnection> Bind() {
    mojo::PendingRemote<blink::mojom::PresentationConnection> connection;
    receiver_connection_receiver_.Bind(
        connection.InitWithNewPipeAndPassReceiver());
    return connection;
  }

 private:
  mojo::Receiver<blink::mojom::PresentationConnection>
      receiver_connection_receiver_{this};
  mojo::Remote<blink::mojom::PresentationConnection>
      receiver_connection_remote_;
};

// This class is used to wait for Terminate to finish before destroying a
// PresentationReceiverWindowController.  It destroys it as soon as its
// termination callback is called (OnTerminate).  This is in contrast to just
// using a RunLoop with a QuitClosure callback in each test.  The latter allows
// extra events in the RunLoop to be handled before actually destroying the
// PresentationReceiverWindowController, which doesn't test whether it's
// actually safe to destroy it in the termination callback itself.
class ReceiverWindowDestroyer {
 public:
  ReceiverWindowDestroyer() = default;
  ~ReceiverWindowDestroyer() = default;

  void AwaitTerminate(
      std::unique_ptr<PresentationReceiverWindowController> receiver_window) {
    receiver_window_ = std::move(receiver_window);
    receiver_window_->Terminate();
    run_loop_.Run();
  }

  void OnTerminate() {
    receiver_window_.reset();
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  std::unique_ptr<PresentationReceiverWindowController> receiver_window_;
};

}  // namespace

class PresentationReceiverWindowControllerBrowserTest
    : public InProcessBrowserTest {
 protected:
  void CloseWindow(PresentationReceiverWindowController* receiver_window) {
    receiver_window->CloseWindowForTest();
  }

  bool IsWindowFullscreen(
      const PresentationReceiverWindowController& receiver_window) {
    return receiver_window.IsWindowFullscreenForTest();
  }

  bool IsWindowActive(
      const PresentationReceiverWindowController& receiver_window) {
    return receiver_window.IsWindowActiveForTest();
  }

  gfx::Rect GetWindowBounds(
      const PresentationReceiverWindowController& receiver_window) {
    return receiver_window.GetWindowBoundsForTest();
  }
};

IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowControllerBrowserTest,
                       CreatesWindow) {
  ReceiverWindowDestroyer destroyer;
  auto receiver_window =
      PresentationReceiverWindowController::CreateFromOriginalProfile(
          browser()->profile(), gfx::Rect(100, 100),
          base::BindOnce(&ReceiverWindowDestroyer::OnTerminate,
                         base::Unretained(&destroyer)),
          GetNoopTitleChangeCallback());
  receiver_window->Start(kPresentationId, GURL("about:blank"));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return IsWindowFullscreen(*receiver_window); }));

  destroyer.AwaitTerminate(std::move(receiver_window));
}

IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowControllerBrowserTest,
                       MANUAL_CreatesWindowOnGivenDisplay) {
  // Pick specific display.
  auto* screen = display::Screen::Get();
  const auto& displays = screen->GetAllDisplays();
  for (const auto& display : displays) {
    DVLOG(0) << display.ToString();
  }

  // Choose a non-default display to which to move the receiver window.
  ASSERT_LE(2ul, displays.size());
  const auto default_display = screen->GetDisplayNearestWindow(
      browser()->GetWindow()->GetNativeWindow());
  display::Display target_display;
  ASSERT_FALSE(target_display.is_valid());
  for (const auto& display : displays) {
    if (display.id() != default_display.id()) {
      target_display = display;
      break;
    }
  }
  ASSERT_TRUE(target_display.is_valid());

  ReceiverWindowDestroyer destroyer;
  auto receiver_window =
      PresentationReceiverWindowController::CreateFromOriginalProfile(
          browser()->profile(), target_display.bounds(),
          base::BindOnce(&ReceiverWindowDestroyer::OnTerminate,
                         base::Unretained(&destroyer)),
          GetNoopTitleChangeCallback());
  receiver_window->Start(kPresentationId, GURL("about:blank"));
  ASSERT_TRUE(content::WaitForLoadStop(receiver_window->web_contents()));

  // Check the window is on the correct display.
  const auto display =
      screen->GetDisplayMatching(GetWindowBounds(*receiver_window));
  EXPECT_EQ(display.id(), target_display.id());

  // The inactive test won't work on single-display systems because fullscreen
  // forces it to have focus, so it must be part of the manual test with 2+
  // displays.
  EXPECT_FALSE(IsWindowActive(*receiver_window));

  destroyer.AwaitTerminate(std::move(receiver_window));
}

// Flaky. See https://crbug.com/41411389.
IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowControllerBrowserTest,
                       DISABLED_NavigationClosesWindow) {
  // Start receiver window.
  auto file_path =
      GetResourceFile(FILE_PATH_LITERAL("presentation_receiver.html"));
  ASSERT_FALSE(file_path.empty());
  const GURL presentation_url = net::FilePathToFileURL(file_path);
  ReceiverWindowDestroyer destroyer;
  auto receiver_window =
      PresentationReceiverWindowController::CreateFromOriginalProfile(
          browser()->profile(), gfx::Rect(100, 100),
          base::BindOnce(&ReceiverWindowDestroyer::OnTerminate,
                         base::Unretained(&destroyer)),
          GetNoopTitleChangeCallback());
  receiver_window->Start(kPresentationId, presentation_url);
  ASSERT_TRUE(content::WaitForLoadStop(receiver_window->web_contents()));

  content::WebContentsDestroyedWatcher destroyed_watcher(
      receiver_window->web_contents());
  ASSERT_TRUE(content::ExecJs(receiver_window->web_contents(),
                              "window.location = 'about:blank'"));
  destroyed_watcher.Wait();

  destroyer.AwaitTerminate(std::move(receiver_window));
}

// Flaky. See https://crbug.com/41387325.
IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowControllerBrowserTest,
                       DISABLED_PresentationApiCommunication) {
  // Start receiver window.
  auto file_path =
      GetResourceFile(FILE_PATH_LITERAL("presentation_receiver.html"));
  ASSERT_FALSE(file_path.empty());
  const GURL presentation_url = net::FilePathToFileURL(file_path);
  ReceiverWindowDestroyer destroyer;
  auto receiver_window =
      PresentationReceiverWindowController::CreateFromOriginalProfile(
          browser()->profile(), gfx::Rect(100, 100),
          base::BindOnce(&ReceiverWindowDestroyer::OnTerminate,
                         base::Unretained(&destroyer)),
          GetNoopTitleChangeCallback());
  receiver_window->Start(kPresentationId, presentation_url);

  // Register controller with LocalPresentationManager using test-local
  // implementation of blink::mojom::PresentationConnection.
  FakeControllerConnection controller_connection;
  auto controller_ptr = controller_connection.Bind();
  media_router::LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
      browser()->profile())
      ->RegisterLocalPresentationController(
          blink::mojom::PresentationInfo(presentation_url, kPresentationId),
          content::GlobalRenderFrameHostId(0, 0), std::move(controller_ptr),
          controller_connection.MakeConnectionRequest(),
          media_router::MediaRoute("route",
                                   media_router::MediaSource(presentation_url),
                                   "sink", "desc", true));

  base::RunLoop connection_loop;
  EXPECT_CALL(controller_connection, OnMessage(_)).WillOnce([&](auto response) {
    ASSERT_TRUE(response->is_message());
    EXPECT_EQ("ready", response->get_message());
    connection_loop.Quit();
  });
  connection_loop.Run();

  // Test ping-pong message.
  const std::string message("turtles");
  base::RunLoop run_loop;
  EXPECT_CALL(controller_connection, OnMessage(_)).WillOnce([&](auto response) {
    ASSERT_TRUE(response->is_message());
    EXPECT_EQ("Pong: " + message, response->get_message());
    run_loop.Quit();
  });
  controller_connection.SendTextMessage(message);
  run_loop.Run();

  destroyer.AwaitTerminate(std::move(receiver_window));
}

class PresentationReceiverNavigationBrowserTest
    : public PresentationReceiverWindowControllerBrowserTest {
 protected:
  PresentationReceiverNavigationBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    PresentationReceiverWindowControllerBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // navigator.presentation is [SecureContext]; serve over HTTPS so the
    // hijacker page's user JS can read the stolen connection.
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/media/router");
    ASSERT_TRUE(https_server_.Start());
  }

  net::EmbeddedTestServer https_server_;
};

// Observes a receiver WebContents and records every committed primary
// main-frame URL until the WebContents is destroyed.
class CommittedUrlRecorder : public content::WebContentsObserver {
 public:
  explicit CommittedUrlRecorder(content::WebContents* wc)
      : content::WebContentsObserver(wc) {}

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (handle->IsInPrimaryMainFrame() && handle->HasCommitted()) {
      committed_urls_.push_back(handle->GetURL());
      LOG(ERROR) << "Main-frame navigation committed: "
                 << handle->GetURL().spec();
      if (on_commit_cb_ && handle->GetURL() == on_commit_url_) {
        std::move(on_commit_cb_).Run();
      }
    }
  }
  void RunOnCommit(const GURL& url, base::OnceClosure cb) {
    on_commit_url_ = url;
    on_commit_cb_ = std::move(cb);
  }
  const std::vector<GURL>& committed_urls() const { return committed_urls_; }

 private:
  std::vector<GURL> committed_urls_;
  GURL on_commit_url_;
  base::OnceClosure on_commit_cb_;
};

IN_PROC_BROWSER_TEST_F(PresentationReceiverNavigationBrowserTest,
                       CrossOriginNavigationDoesNotCommit) {
  // Two distinct HTTPS origins (a.test vs b.test, both covered by
  // CERT_TEST_NAMES) — site isolation puts them in different renderer
  // processes and both are SecureContexts so navigator.presentation is exposed.
  const GURL target_url = https_server_.GetURL("b.test", "/target.html");
  const std::string receiver_path =
      "/target_receiver.html?" +
      base::EscapeQueryParamValue(target_url.spec(), /*use_plus=*/false);
  const GURL start_url = https_server_.GetURL("a.test", receiver_path);
  const url::Origin target_origin = url::Origin::Create(target_url);
  ASSERT_NE(url::Origin::Create(start_url), target_origin);

  // 1. Create the receiver window.
  // Instead of ReceiverWindowDestroyer, we use a simple RunLoop to wait for
  // the asynchronous termination callback.
  base::RunLoop terminate_loop;
  auto receiver_window =
      PresentationReceiverWindowController::CreateFromOriginalProfile(
          browser()->profile(), gfx::Rect(100, 100),
          terminate_loop.QuitClosure(), GetNoopTitleChangeCallback());
  CommittedUrlRecorder recorder(receiver_window->web_contents());
  receiver_window->Start(kPresentationId, start_url);

  // 2. start_url commits and Blink eagerly creates a PresentationReceiver.
  //    start_url then attempts to navigate to target_url.
  //    PresentationNavigationPolicy::AllowNavigation returns false for that
  //    second main-frame navigation.
  //    Our fix asynchronously stops the navigation and terminates the window,
  //    which runs the termination callback and quits the loop.
  terminate_loop.Run();

  // 3. Verify that the disallowed navigation never committed.
  EXPECT_EQ(1u, recorder.committed_urls().size());
  EXPECT_EQ(start_url, recorder.committed_urls()[0]);

  // 4. Register a controller connection for the same presentation_id.
  //    Since the receiver window is destroyed/terminated, the connection
  //    should not be hijacked or routed to target.
  FakeControllerConnection controller_connection;
  media_router::LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
      browser()->profile())
      ->RegisterLocalPresentationController(
          blink::mojom::PresentationInfo(start_url, kPresentationId),
          content::GlobalRenderFrameHostId(0, 0), controller_connection.Bind(),
          controller_connection.MakeConnectionRequest(),
          media_router::MediaRoute("route",
                                   media_router::MediaSource(start_url), "sink",
                                   "desc", true));

  std::string received;
  base::RunLoop loop;
  EXPECT_CALL(controller_connection, OnMessage(_))
      .WillRepeatedly([&](blink::mojom::PresentationConnectionMessagePtr msg) {
        if (msg->is_message()) {
          received = msg->get_message();
        }
        loop.Quit();
      });

  // Run the loop for a short time to ensure no message is received.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(500));
  loop.Run();

  // Safely destroy the receiver window controller.
  receiver_window.reset();

  // 5. Verify that no message to target was received.
  EXPECT_TRUE(received.empty());
}

IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowControllerBrowserTest,
                       WindowClosingTerminatesPresentation) {
  // Start receiver window.
  ReceiverWindowDestroyer destroyer;
  auto receiver_window =
      PresentationReceiverWindowController::CreateFromOriginalProfile(
          browser()->profile(), gfx::Rect(100, 100),
          base::BindOnce(&ReceiverWindowDestroyer::OnTerminate,
                         base::Unretained(&destroyer)),
          GetNoopTitleChangeCallback());
  receiver_window->Start(kPresentationId, GURL("about:blank"));
  ASSERT_TRUE(content::WaitForLoadStop(receiver_window->web_contents()));

  content::WebContentsDestroyedWatcher destroyed_watcher(
      receiver_window->web_contents());
  CloseWindow(receiver_window.get());
  destroyed_watcher.Wait();

  destroyer.AwaitTerminate(std::move(receiver_window));
}
