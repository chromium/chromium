// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/formats/webm/webm_stream_parser.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"

namespace {

// Runs a loop for the given |milliseconds| duration.
void WaitForMilliseconds(int milliseconds) {
#if defined(MEMORY_SANITIZER)
  // MSAN runs much slower than regular tests, so give it more time to complete
  milliseconds *= 2;
#endif

  base::RunLoop loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(milliseconds));
  loop.Run();
}

// Wait for the service to flush all the video chunks to ash, and waits for the
// file contents to be fully saved, and returns the path where the video file
// was saved.
base::FilePath WaitForVideoFileToBeSaved() {
  base::FilePath result;
  base::RunLoop run_loop;
  ash::CaptureModeTestApi().SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        result = path;
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

// Verifies the contents of a WebM file by parsing it.
class WebmVerifier {
 public:
  WebmVerifier() {
    webm_parser_.Init(
        base::BindOnce(&WebmVerifier::OnInit, base::Unretained(this)),
        base::BindRepeating(&WebmVerifier::OnNewConfig, base::Unretained(this)),
        base::BindRepeating(&WebmVerifier::OnNewBuffers,
                            base::Unretained(this)),
        /*ignore_text_tracks=*/true,
        base::BindRepeating(&WebmVerifier::OnEncryptedMediaInitData,
                            base::Unretained(this)),
        base::BindRepeating(&WebmVerifier::OnNewMediaSegment,
                            base::Unretained(this)),
        base::BindRepeating(&WebmVerifier::OnEndMediaSegment,
                            base::Unretained(this)),
        &media_log_);
  }
  WebmVerifier(const WebmVerifier&) = delete;
  WebmVerifier& operator=(const WebmVerifier&) = delete;
  ~WebmVerifier() = default;

  // Parses the given |webm_file_content| and returns true on success.
  bool Verify(const std::string& webm_file_content) {
    if (!webm_parser_.AppendToParseBuffer(
            reinterpret_cast<const uint8_t*>(webm_file_content.data()),
            webm_file_content.size())) {
      return false;
    }

    // Run the segment parser loop one time with the full size of the appended
    // data to ensure the parser has had a chance to parse all the appended
    // bytes.
    media::StreamParser::ParseStatus result =
        webm_parser_.Parse(webm_file_content.size());

    // Note that media::StreamParser::ParseStatus::kSuccessHasMoreData is deemed
    // a verification failure here, since the parser was told to parse all the
    // appended bytes and should not have uninspected data afterwards.
    return result == media::StreamParser::ParseStatus::kSuccess;
  }

 private:
  void OnInit(const media::StreamParser::InitParameters&) {}
  bool OnNewConfig(std::unique_ptr<media::MediaTracks> tracks,
                   const media::StreamParser::TextTrackConfigMap&) {
    return true;
  }
  bool OnNewBuffers(const media::StreamParser::BufferQueueMap& map) {
    return true;
  }
  void OnEncryptedMediaInitData(media::EmeInitDataType,
                                const std::vector<uint8_t>&) {}
  void OnNewMediaSegment() {}
  void OnEndMediaSegment() {}

  media::WebMStreamParser webm_parser_;
  media::MockMediaLog media_log_;
};

}  // namespace

class RecordingServiceBrowserTest : public InProcessBrowserTest {
 public:
  RecordingServiceBrowserTest() = default;
  RecordingServiceBrowserTest(const RecordingServiceBrowserTest&) = delete;
  RecordingServiceBrowserTest& operator=(const RecordingServiceBrowserTest&) =
      delete;
  ~RecordingServiceBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    aura::Window* browser_window = GetBrowserWindow();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        browser_window->GetRootWindow(), browser_window);
    // To improve the test efficiency, we set the display to a small size.
    display::test::DisplayManagerTestApi(ash::ShellTestApi().display_manager())
        .UpdateDisplay("300x200");
    // To avoid flaky tests, we disable audio recording, since the bots won't
    // capture any audio and won't produce any audio frames. This will cause the
    // muxer to discard video frames if it expects audio frames but got none,
    // which may cause the produced webm file to be empty. See issues
    // https://crbug.com/1151167 and https://crbug.com/1151418.
    ash::CaptureModeTestApi().SetAudioRecordingEnabled(false);
  }

  aura::Window* GetBrowserWindow() const {
    return browser()->window()->GetNativeWindow();
  }

  ui::test::EventGenerator* GetEventGenerator() {
    return event_generator_.get();
  }

  // Reads the video file at the given |path| and verifies its WebM contents. At
  // the end it deletes the file to save space, since video files can be big.
  // |allow_empty| can be set to true if an empty video file is possible.
  void VerifyVideoFileAndDelete(const base::FilePath& path,
                                bool allow_empty = false) const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathExists(path));
    std::string file_content;
    EXPECT_TRUE(base::ReadFileToString(path, &file_content));

    if (allow_empty && file_content.empty())
      return;

    EXPECT_FALSE(file_content.empty());
    EXPECT_TRUE(WebmVerifier().Verify(file_content));
    EXPECT_TRUE(base::DeleteFile(path));
  }

  void FinishVideoRecordingTest(ash::CaptureModeTestApi* test_api) {
    test_api->PerformCapture();
    test_api->FlushRecordingServiceForTesting();
    // Record a 1.5-second long video to give it enough time to produce and send
    // video frames in order to exercise all the code paths of the service and
    // its client.
    WaitForMilliseconds(1500);
    test_api->StopVideoRecording();
    const base::FilePath video_path = WaitForVideoFileToBeSaved();
    VerifyVideoFileAndDelete(video_path);
  }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest, RecordFullscreen) {
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  FinishVideoRecordingTest(&test_api);
}

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest, RecordWindow) {
  ash::CaptureModeTestApi test_api;
  test_api.StartForWindow(/*for_video=*/true);
  auto* generator = GetEventGenerator();
  // Move the mouse cursor above the browser window to select it for window
  // capture (make sure it doesn't hover over the capture bar).
  generator->MoveMouseTo(GetBrowserWindow()->GetBoundsInScreen().top_center());
  FinishVideoRecordingTest(&test_api);
}

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest, RecordWindowMultiDisplay) {
  display::test::DisplayManagerTestApi(ash::ShellTestApi().display_manager())
      .UpdateDisplay("300x200,301+0-400x400");

  ash::CaptureModeTestApi capture_mode_test_api;
  capture_mode_test_api.StartForWindow(/*for_video=*/true);
  auto* generator = GetEventGenerator();
  // Move the mouse cursor above the browser window to select it for window
  // capture (make sure it doesn't hover over the capture bar).
  generator->MoveMouseTo(GetBrowserWindow()->GetBoundsInScreen().top_center());
  capture_mode_test_api.PerformCapture();
  capture_mode_test_api.FlushRecordingServiceForTesting();

  // Moves the browser window to the display at the given |screen_point|.
  auto move_browser_to_display_at_point = [&](const gfx::Point& screen_point) {
    auto* screen = display::Screen::GetScreen();
    aura::Window* new_root =
        screen->GetWindowAtScreenPoint(screen_point)->GetRootWindow();
    auto* browser_window = GetBrowserWindow();
    EXPECT_NE(new_root, browser_window->GetRootWindow());
    auto* target_container =
        new_root->GetChildById(browser_window->parent()->GetId());
    DCHECK(target_container);
    target_container->AddChild(browser_window);
    EXPECT_EQ(new_root, browser_window->GetRootWindow());
  };

  // Record for a little bit, then move the window to the secondary display.
  WaitForMilliseconds(600);
  move_browser_to_display_at_point(gfx::Point(320, 50));
  capture_mode_test_api.FlushRecordingServiceForTesting();

  // Record for a little bit, then resize the browser window.
  WaitForMilliseconds(600);
  GetBrowserWindow()->SetBounds(gfx::Rect(310, 10, 300, 300));
  capture_mode_test_api.FlushRecordingServiceForTesting();

  // Record for a little bit, then move the browser window back to the smaller
  // display.
  WaitForMilliseconds(600);
  move_browser_to_display_at_point(gfx::Point(0, 0));
  capture_mode_test_api.FlushRecordingServiceForTesting();

  // Record for a little bit, then end recording. The output video file should
  // still be valid.
  WaitForMilliseconds(600);
  capture_mode_test_api.StopVideoRecording();
  const base::FilePath video_path = WaitForVideoFileToBeSaved();
  VerifyVideoFileAndDelete(video_path);
}

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest, RecordRegion) {
  ash::CaptureModeTestApi test_api;
  test_api.StartForRegion(/*for_video=*/true);
  // Select a random partial region of the screen.
  test_api.SetUserSelectedRegion(gfx::Rect(10, 20, 100, 50));
  FinishVideoRecordingTest(&test_api);
}

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest,
                       RecordingServiceEndpointDropped) {
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  test_api.PerformCapture();
  test_api.FlushRecordingServiceForTesting();
  WaitForMilliseconds(1000);
  test_api.ResetRecordingServiceRemote();
  const base::FilePath video_path = WaitForVideoFileToBeSaved();
  VerifyVideoFileAndDelete(video_path);
}

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest,
                       RecordingServiceClientEndpointDropped) {
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  test_api.PerformCapture();
  test_api.FlushRecordingServiceForTesting();
  WaitForMilliseconds(1000);
  test_api.ResetRecordingServiceClientReceiver();
  const base::FilePath video_path = WaitForVideoFileToBeSaved();
  // Due to buffering on the service side, the channel might get dropped before
  // any flushing of those beffers ever happens, and since dropping the client
  // end point will immediately terminate the service, nothing may ever get
  // flushed, and the resulting video file can be empty.
  VerifyVideoFileAndDelete(video_path, /*allow_empty=*/true);
}

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest, SuccessiveRecording) {
  ash::CaptureModeTestApi test_api;
  // Do a fullscreen recording, followed by a region recording.
  test_api.StartForFullscreen(/*for_video=*/true);
  FinishVideoRecordingTest(&test_api);
  test_api.StartForRegion(/*for_video=*/true);
  test_api.SetUserSelectedRegion(gfx::Rect(50, 200));
  FinishVideoRecordingTest(&test_api);
}

// Tests that recording will be interrupted once screen capture becomes locked.
IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest,
                       RecordingInterruptedOnCaptureLocked) {
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  test_api.PerformCapture();
  test_api.FlushRecordingServiceForTesting();
  WaitForMilliseconds(1000);
  ChromeCaptureModeDelegate::Get()->SetIsScreenCaptureLocked(true);
  const base::FilePath video_path = WaitForVideoFileToBeSaved();
  VerifyVideoFileAndDelete(video_path);
}

IN_PROC_BROWSER_TEST_F(RecordingServiceBrowserTest, InvalidDownloadsPath) {
  auto* download_prefs =
      DownloadPrefs::FromBrowserContext(browser()->profile());
  const base::FilePath removable_path =
      ash::CrosDisksClient::GetRemovableDiskMountPoint();
  const base::FilePath invalid_path =
      removable_path.Append(FILE_PATH_LITERAL("backup"));
  download_prefs->SetDownloadPath(invalid_path);
  // The invalid path will still be accepted by the browser, but won't be used
  // by Capture Mode.
  EXPECT_EQ(invalid_path, download_prefs->DownloadPath());
  EXPECT_NE(invalid_path,
            ChromeCaptureModeDelegate::Get()->GetUserDefaultDownloadsFolder());
  ash::CaptureModeTestApi test_api;
  test_api.StartForFullscreen(/*for_video=*/true);
  FinishVideoRecordingTest(&test_api);
}
