// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_screenshotter.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/recording_map.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/geometry/rect.h"

constexpr size_t kMaxScreenshotFileSize = 50 * 1000L * 1000L;  // 50 MB.

namespace {

int debug_file_sequencer = 0;

void WriteBitmapToPng(const SkBitmap& bitmap) {
  base::FilePath temp_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  std::string screenshot_filename = base::StringPrintf(
      "csai_main_content_web_screenshot_%i.png", debug_file_sequencer++);
  std::string screenshot_filepath =
      temp_dir.AppendASCII(screenshot_filename).MaybeAsASCII();
  SkFILEWStream out_file(screenshot_filepath.c_str());
  if (!out_file.isValid()) {
    VLOG(2) << "Unable to create: " << screenshot_filepath;
    return;
  }

  bool success =
      SkPngEncoder::Encode(&out_file, bitmap.pixmap(), /*options=*/{});
  if (success) {
    VLOG(2) << "Wrote debug file: " << screenshot_filepath;
  } else {
    VLOG(2) << "Failed to write debug file: " << screenshot_filepath;
  }
}

}  // namespace

ReadAnythingScreenshotter::ReadAnythingScreenshotter()
    : paint_preview::PaintPreviewBaseService(
          /*file_mixin=*/nullptr,  // in-memory captures
          /*policy=*/nullptr,      // all content is deemed amenable
          /*is_off_the_record=*/false),
      paint_preview_compositor_service_(nullptr,
                                        base::OnTaskRunnerDeleter(nullptr)),
      paint_preview_compositor_client_(nullptr,
                                       base::OnTaskRunnerDeleter(nullptr)) {
  paint_preview_compositor_service_ =
      paint_preview::StartCompositorService(base::BindOnce(
          &ReadAnythingScreenshotter::OnCompositorServiceDisconnected,
          weak_ptr_factory_.GetWeakPtr()));
  CHECK(paint_preview_compositor_service_);
}

ReadAnythingScreenshotter::~ReadAnythingScreenshotter() = default;

void ReadAnythingScreenshotter::RequestScreenshot(
    const raw_ptr<content::WebContents> web_contents) {
  if (!web_contents) {
    VLOG(2) << "The given web contents no longer valid";
    return;
  }

  // Start capturing via Paint Preview.
  CaptureParams capture_params;
  capture_params.web_contents = web_contents;
  capture_params.persistence =
      paint_preview::RecordingPersistence::kMemoryBuffer;
  capture_params.max_per_capture_size = kMaxScreenshotFileSize;
  CapturePaintPreview(
      capture_params,
      base::BindOnce(&ReadAnythingScreenshotter::OnScreenshotCaptured,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ReadAnythingScreenshotter::OnScreenshotCaptured(
    paint_preview::PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<paint_preview::CaptureResult> result) {
  if (status != PaintPreviewBaseService::CaptureStatus::kOk ||
      !result->capture_success) {
    VLOG(2) << base::StringPrintf(
        "Failed to capture a screenshot (CaptureStatus=%d)",
        static_cast<int>(status));
    return;
  }
  if (!paint_preview_compositor_client_) {
    paint_preview_compositor_client_ =
        paint_preview_compositor_service_->CreateCompositor(
            base::BindOnce(&ReadAnythingScreenshotter::SendCompositeRequest,
                           weak_ptr_factory_.GetWeakPtr(),
                           PrepareCompositeRequest(std::move(result))));
  } else {
    SendCompositeRequest(PrepareCompositeRequest(std::move(result)));
  }
}

paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
ReadAnythingScreenshotter::PrepareCompositeRequest(
    std::unique_ptr<paint_preview::CaptureResult> capture_result) {
  paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
      begin_composite_request =
          paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
  std::pair<paint_preview::RecordingMap, paint_preview::PaintPreviewProto>
      map_and_proto = paint_preview::RecordingMapFromCaptureResult(
          std::move(*capture_result));
  begin_composite_request->recording_map = std::move(map_and_proto.first);
  if (begin_composite_request->recording_map.empty()) {
    VLOG(2) << "Captured an empty screenshot";
    return nullptr;
  }
  begin_composite_request->preview =
      mojo_base::ProtoWrapper(std::move(map_and_proto.second));
  return begin_composite_request;
}

void ReadAnythingScreenshotter::SendCompositeRequest(
    paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
        begin_composite_request) {
  if (!begin_composite_request) {
    VLOG(2) << "Invalid begin_composite_request";
    return;
  }

  CHECK(paint_preview_compositor_client_);
  paint_preview_compositor_client_->BeginMainFrameComposite(
      std::move(begin_composite_request),
      base::BindOnce(&ReadAnythingScreenshotter::OnCompositeFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ReadAnythingScreenshotter::OnCompositorServiceDisconnected() {
  VLOG(2) << "Compositor service is disconnected";
  paint_preview_compositor_client_.reset();
  paint_preview_compositor_service_.reset();
}

void ReadAnythingScreenshotter::OnCompositeFinished(
    paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
    paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr response) {
  if (status != paint_preview::mojom::PaintPreviewCompositor::
                    BeginCompositeStatus::kSuccess &&
      status != paint_preview::mojom::PaintPreviewCompositor::
                    BeginCompositeStatus::kPartialSuccess) {
    VLOG(2) << base::StringPrintf(
        "Failed to composite (BeginCompositeStatus=%d)",
        static_cast<int>(status));
    return;
  }
  // Start converting to a bitmap.
  RequestBitmapForMainFrame();
}

void ReadAnythingScreenshotter::RequestBitmapForMainFrame() {
  // Passing an empty `gfx::Rect` allows us to get a bitmap for the full page.
  paint_preview_compositor_client_->BitmapForMainFrame(
      gfx::Rect(), /*scale_factor=*/1.0,
      base::BindOnce(&ReadAnythingScreenshotter::OnBitmapReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ReadAnythingScreenshotter::OnBitmapReceived(
    paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
    const SkBitmap& bitmap) {
  if (status != paint_preview::mojom::PaintPreviewCompositor::BitmapStatus::
                    kSuccess ||
      bitmap.empty()) {
    VLOG(2) << base::StringPrintf("Failed to get bitmap (BitmapStatus=%d)",
                                  static_cast<int>(status));
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&WriteBitmapToPng, bitmap));
}
