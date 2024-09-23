// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/base/save_desktop_snapshot.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/public/browser/desktop_capture.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/codec/png_codec.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

// A callback that holds the last frame captured by a webrtc::DesktopCapturer.
class FrameHolder : public webrtc::DesktopCapturer::Callback {
 public:
  FrameHolder() = default;
  FrameHolder(const FrameHolder&) = delete;
  FrameHolder& operator=(const FrameHolder&) = delete;

  // Returns the frame that was captured or null in case of failure.
  std::unique_ptr<webrtc::DesktopFrame> TakeFrame() {
    CHECK(signal_.Wait());
    return std::move(frame_);
  }

 private:
  // webrtc::DesktopCapturer::Callback:
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override {
    if (result == webrtc::DesktopCapturer::Result::SUCCESS)
      frame_ = std::move(frame);
    signal_.SetValue();
  }

  std::unique_ptr<webrtc::DesktopFrame> frame_;
  base::test::TestFuture<void> signal_;
};

// Captures and returns a snapshot of the screen, or an empty bitmap in case of
// error.
SkBitmap CaptureScreen() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::ScreenManager>()) {
    LOG(WARNING) << "crosapi must be available to CreateScreenCapturer.";
    return SkBitmap();
  }
#endif

  std::unique_ptr<webrtc::DesktopCapturer> capturer =
      content::desktop_capture::CreateScreenCapturer();
  if (!capturer) {
    LOG(ERROR) << "Failed to create a screen capturer.";
    return SkBitmap();
  }

  // Grab a single frame.
  FrameHolder frame_holder;
  capturer->Start(&frame_holder);
  capturer->CaptureFrame();
  std::unique_ptr<webrtc::DesktopFrame> frame(frame_holder.TakeFrame());

  if (!frame) {
    LOG(ERROR) << "Failed to capture a frame of the screen for a snapshot.";
    return SkBitmap();
  }

  // Create an image from the frame.
  SkBitmap result;
  result.allocN32Pixels(frame->size().width(), frame->size().height(), true);
  memcpy(result.getAddr32(0, 0), frame->data(),
         frame->size().width() * frame->size().height() *
             webrtc::DesktopFrame::kBytesPerPixel);
  return result;
}

}  // namespace

const char kSnapshotOutputDir[] = "snapshot-output-dir";

base::FilePath SaveDesktopSnapshot() {
  const base::FilePath output_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kSnapshotOutputDir);
  if (output_dir.empty())
    return base::FilePath();
  return SaveDesktopSnapshot(output_dir);
}

base::FilePath SaveDesktopSnapshot(const base::FilePath& output_dir) {
  // Take the snapshot and encode it.
  SkBitmap screen = CaptureScreen();
  if (screen.drawsNothing())
    return base::FilePath();

  std::vector<unsigned char> encoded;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(CaptureScreen(), false, &encoded)) {
    LOG(ERROR) << "Failed to PNG encode screen snapshot.";
    return base::FilePath();
  }

  // Create the output file.
  base::FilePath output_path =
      output_dir.AppendASCII(base::UnlocalizedTimeFormatWithPattern(
          base::Time::Now(), "'ss_'yyyyMMddHHmmss_SSS'.png'"));
  uint32_t flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE;
#if BUILDFLAG(IS_WIN)
  flags |=
      base::File::FLAG_WIN_SHARE_DELETE | base::File::FLAG_CAN_DELETE_ON_CLOSE;
#endif
  base::File file(output_path, flags);

  if (!file.IsValid()) {
    if (file.error_details() == base::File::FILE_ERROR_EXISTS) {
      LOG(INFO) << "Skipping screen snapshot since it is already present: "
                << output_path.BaseName();
    } else {
      LOG(ERROR) << "Failed to create snapshot output file \"" << output_path
                 << "\" with error " << file.error_details();
    }
    return base::FilePath();
  }

  // Write it to disk.
  if (!file.WriteAtCurrentPosAndCheck(encoded)) {
    LOG(ERROR) << "Failed to write entire snapshot to file";
    return base::FilePath();
  }

  return output_path;
}
