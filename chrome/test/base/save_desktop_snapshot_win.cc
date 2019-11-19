// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/save_desktop_snapshot_win.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

// A callback that holds the last frame catpured by a webrtc::DesktopCapturer.
class FrameHolder : public webrtc::DesktopCapturer::Callback {
 public:
  FrameHolder() = default;

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
  DISALLOW_COPY_AND_ASSIGN(FrameHolder);
};

// Captures and returns a snapshot of the screen, or an empty bitmap in case of
// error.
SkBitmap CaptureScreen() {
  auto options = webrtc::DesktopCaptureOptions::CreateDefault();
  options.set_disable_effects(false);
  options.set_allow_directx_capturer(true);
  options.set_allow_use_magnification_api(false);
  std::unique_ptr<webrtc::DesktopCapturer> capturer(
      webrtc::DesktopCapturer::CreateScreenCapturer(options));

  // Grab a single frame.
  FrameHolder frame_holder;
  capturer->Start(&frame_holder);
  capturer->CaptureFrame();
  std::unique_ptr<webrtc::DesktopFrame> frame(frame_holder.TakeFrame());

  if (!frame)
    return SkBitmap();

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

base::FilePath SaveDesktopSnapshot(const base::FilePath& output_dir) {
  // Create the output file.
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  base::FilePath output_path(
      output_dir.Append(base::FilePath(base::StringPrintf(
          L"ss_%4d%02d%02d%02d%02d%02d_%03d.png", exploded.year, exploded.month,
          exploded.day_of_month, exploded.hour, exploded.minute,
          exploded.second, exploded.millisecond))));
  base::File file(output_path, base::File::FLAG_CREATE |
                                   base::File::FLAG_WRITE |
                                   base::File::FLAG_SHARE_DELETE |
                                   base::File::FLAG_CAN_DELETE_ON_CLOSE);
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

  // Delete the output file in case of any error.
  file.DeleteOnClose(true);

  // Take the snapshot and encode it.
  SkBitmap screen = CaptureScreen();
  if (screen.drawsNothing()) {
    LOG(ERROR) << "Failed to capture a frame of the screen for a snapshot.";
    return base::FilePath();
  }

  std::vector<unsigned char> encoded;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(CaptureScreen(), false, &encoded)) {
    LOG(ERROR) << "Failed to PNG encode screen snapshot.";
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

  // Keep the output file.
  file.DeleteOnClose(false);
  return output_path;
}
