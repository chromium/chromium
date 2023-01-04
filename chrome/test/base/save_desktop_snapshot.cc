// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/save_desktop_snapshot.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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
    return std::move(frame_);
  }

 private:
  // webrtc::DesktopCapturer::Callback:
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override {
    if (result == webrtc::DesktopCapturer::Result::SUCCESS)
      frame_ = std::move(frame);
  }

  std::unique_ptr<webrtc::DesktopFrame> frame_;
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
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  const auto filename = base::StringPrintf(
      "ss_%4d%02d%02d%02d%02d%02d_%03d.png", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);
#if BUILDFLAG(IS_WIN)
  base::FilePath output_path(output_dir.Append(base::UTF8ToWide(filename)));
  base::File file(output_path, base::File::FLAG_CREATE |
                                   base::File::FLAG_WRITE |
                                   base::File::FLAG_WIN_SHARE_DELETE |
                                   base::File::FLAG_CAN_DELETE_ON_CLOSE);
#else
  base::FilePath output_path(output_dir.Append(filename));
  base::File file(output_path,
                  base::File::FLAG_CREATE | base::File::FLAG_WRITE);
#endif

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
  const int to_write = base::checked_cast<int>(encoded.size());
  int written =
      file.WriteAtCurrentPos(reinterpret_cast<char*>(encoded.data()), to_write);
  if (written != to_write) {
    LOG(ERROR) << "Failed to write entire snapshot to file";
    return base::FilePath();
  }

  return output_path;
}
