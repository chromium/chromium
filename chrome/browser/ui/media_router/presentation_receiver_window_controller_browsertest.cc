// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/presentation_receiver_window_controller.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/media/router/presentation/local_presentation_manager.h"
#include "chrome/browser/media/router/presentation/local_presentation_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/script_executor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;

namespace {

constexpr char kPresentationId[] = "test_id";
const base::FilePath::StringPieceType kResourcePath =
    FILE_PATH_LITERAL("media/router/");

base::RepeatingCallback<void(const std::string&)> GetNoopTitleChangeCallback() {
  return base::BindRepeating([](const std::string& title) {});
}

base::FilePath GetResourceFile(base::FilePath::StringPieceType relative_path) {
  base::FilePath base_dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &base_dir))
    return base::FilePath();
  base::FilePath full_path =
      base_dir.Append(kResourcePath).Append(relative_path);
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    if (!PathExists(full_path))
      return base::FilePath();
  }
  return full_path;
}

// This class waits for a WebContents it is assigned via Observe to be destroyed
// and then quits a RunLoop it is given.  This is used in tests to wait for the
// receiver page to be torn down in the presentation window.
class CloseObserver final : public content::WebContentsObserver {
 public:
  explicit CloseObserver(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override { run_loop_->Quit(); }

  using content::WebContentsObserver::Observe;

 private:
  base::RunLoop* const run_loop_;

  DISALLOW_COPY_AND_ASSIGN(CloseObserver);
};

// This class imitates a presentation controller page from a messaging
// standpoint.  It is registered as a controller connection for the appropriate
// presentation ID with the LocalPresentationManager to facilitate a
// presentation API communication test with the receiver window.
class FakeControllerConnection final
    : public blink::mojom::PresentationConnection {
 public:
  FakeControllerConnection() {}

  void SendTextMessage(const std::string& message) {
    ASSERT_TRUE(receiver_connection_remote_.is_bound());
    receiver_connection_remote_->OnMessage(
        blink::mojom::PresentationConnectionMessage::NewMessage(message));
  }

  // blink::mojom::PresentationConnection implementation
  MOCK_METHOD1(OnMessage,
               void(blink::mojom::PresentationConnectionMessagePtr message));
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

  DISALLOW_COPY_AND_ASSIGN(FakeControllerConnection);
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
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsWindowFullscreen(*receiver_window));

  destroyer.AwaitTerminate(std::move(receiver_window));
}

IN_PROC_BROWSER_TEST_F(PresentationReceiverWindowControllerBrowserTest,
                       MANUAL_CreatesWindowOnGivenDisplay) {
  // Pick specific display.
  auto* screen = display::Screen::GetScreen();
  const auto& displays = screen->GetAllDisplays();
  for (const auto& display : displays) {
    DVLOG(0) << display.ToString();
  }

  // Choose a non-default display to which to move the receiver window.
  ASSERT_LE(2ul, displays.size());
  const auto default_display =
      screen->GetDisplayNearestWindow(browser()->window()->GetNativeWindow());
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

// Flaky. See https://crbug.com/880045.
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

  base::RunLoop run_loop;
  CloseObserver close_observer(&run_loop);
  close_observer.Observe(receiver_window->web_contents());

  ASSERT_TRUE(content::ExecuteScript(receiver_window->web_contents(),
                                     "window.location = 'about:blank'"));
  run_loop.Run();

  destroyer.AwaitTerminate(std::move(receiver_window));
}

// Flaky. See https://crbug.com/840136.
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
          content::GlobalFrameRoutingId(0, 0), std::move(controller_ptr),
          controller_connection.MakeConnectionRequest(),
          media_router::MediaRoute("route",
                                   media_router::MediaSource(presentation_url),
                                   "sink", "desc", true, true));

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

  base::RunLoop run_loop;
  CloseObserver close_observer(&run_loop);
  close_observer.Observe(receiver_window->web_contents());

  CloseWindow(receiver_window.get());
  run_loop.Run();

  destroyer.AwaitTerminate(std::move(receiver_window));
}
