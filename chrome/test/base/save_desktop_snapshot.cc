// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/save_desktop_snapshot.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/public/browser/desktop_capture.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_span_util.h"

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
  std::unique_ptr<webrtc::DesktopCapturer> capturer =
      content::desktop_capture::CreateScreenCapturer(
          content::desktop_capture::CreateDesktopCaptureOptions(),
          /*for_snapshot=*/true);
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
  // TODO(crbug.com/352187279): Support other pixel formats.
  CHECK_EQ(frame->pixel_format(), webrtc::FOURCC_ARGB);

  if (frame->size().is_empty()) {
    LOG(ERROR) << "Captured frame is empty: " << frame->size().width() << "x"
               << frame->size().height();
    return SkBitmap();
  }

  result.allocN32Pixels(frame->size().width(), frame->size().height(), true);

  SkPixmap pixmap;
  CHECK(result.peekPixels(&pixmap));

  int width = frame->size().width();
  int height = frame->size().height();
  size_t row_bytes =
      static_cast<size_t>(width * webrtc::DesktopFrame::kBytesPerPixel);

  // SAFETY: `frame->data()` is guaranteed by the `webrtc::DesktopFrame` API
  // contract to contain a valid buffer of size `stride() * height()`.
  size_t src_buffer_size = base::CheckMul(static_cast<size_t>(frame->stride()),
                                          static_cast<size_t>(height))
                               .ValueOrDie();
  auto src_span = UNSAFE_BUFFERS(
      base::span(base::unchecked, frame->data(), src_buffer_size));
  auto dest_span = gfx::SkPixmapToWritableSpan(pixmap);
  if (dest_span.empty()) {
    LOG(ERROR) << "Failed to get writable span from pixmap.";
    return SkBitmap();
  }

  for (int y = 0; y < height; ++y) {
    size_t src_offset = static_cast<size_t>(y * frame->stride());
    size_t dest_offset = static_cast<size_t>(y * pixmap.rowBytes());

    auto src_row = src_span.subspan(src_offset, row_bytes);
    auto dest_row = dest_span.subspan(dest_offset, row_bytes);
    dest_row.copy_from(src_row);
  }

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
  if (screen.drawsNothing()) {
    return base::FilePath();
  }

  std::optional<std::vector<uint8_t>> encoded =
      gfx::PNGCodec::EncodeBGRASkBitmap(screen,
                                        /*discard_transparency=*/false);
  if (!encoded) {
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
  if (!file.WriteAtCurrentPosAndCheck(encoded.value())) {
    LOG(ERROR) << "Failed to write entire snapshot to file";
    return base::FilePath();
  }

  return output_path;
}
