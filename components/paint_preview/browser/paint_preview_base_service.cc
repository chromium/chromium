// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

PaintPreviewBaseService::PaintPreviewBaseService(
    std::unique_ptr<PaintPreviewFileMixin> file_mixin,
    std::unique_ptr<PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : file_mixin_(std::move(file_mixin)),
      policy_(std::move(policy)),
      is_off_the_record_(is_off_the_record) {}

PaintPreviewBaseService::~PaintPreviewBaseService() = default;

void PaintPreviewBaseService::CapturePaintPreview(CaptureParams capture_params,
                                                  OnCapturedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = capture_params.web_contents;
  content::RenderFrameHost* render_frame_host =
      capture_params.render_frame_host ? capture_params.render_frame_host.get()
                                       : web_contents->GetPrimaryMainFrame();
  if (policy_ && !policy_->SupportedForContents(web_contents)) {
    std::move(callback).Run(CaptureStatus::kContentUnsupported, {});
    return;
  }

  PaintPreviewClient::CreateForWebContents(web_contents);  // Is a singleton.
  auto* client = PaintPreviewClient::FromWebContents(web_contents);
  if (!client) {
    std::move(callback).Run(CaptureStatus::kClientCreationFailed, {});
    return;
  }

  PaintPreviewClient::PaintPreviewParams params(capture_params.persistence);
  if (capture_params.root_dir) {
    params.root_dir = *capture_params.root_dir;
  }
  params.inner.clip_rect = capture_params.clip_rect;
  params.inner.is_main_frame = render_frame_host->IsInPrimaryMainFrame();
  params.inner.capture_links = capture_params.capture_links;
  params.inner.max_capture_size = capture_params.max_per_capture_size;
  params.inner.max_decoded_image_size_bytes =
      capture_params.max_decoded_image_size_bytes;
  params.inner.skip_accelerated_content =
      capture_params.skip_accelerated_content;

  // TODO(crbug.com/40123632): Consider moving to client so that this always
  // happens. Although, it is harder to get this right in the client due to its
  // lifecycle.
  auto capture_handle = web_contents->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/true,
      /*stay_awake=*/true, /*is_activity=*/true);

  auto start_time = base::TimeTicks::Now();
  client->CapturePaintPreview(
      params, render_frame_host,
      base::BindOnce(&PaintPreviewBaseService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(), std::move(capture_handle),
                     start_time, std::move(callback)));
}

void PaintPreviewBaseService::OnCaptured(
    base::ScopedClosureRunner capture_handle,
    base::TimeTicks start_time,
    OnCapturedCallback callback,
    base::UnguessableToken guid,
    mojom::PaintPreviewStatus status,
    std::unique_ptr<CaptureResult> result) {
  capture_handle.RunAndReset();

  if (!(status == mojom::PaintPreviewStatus::kOk ||
        status == mojom::PaintPreviewStatus::kPartialSuccess) ||
      !result->capture_success) {
    DVLOG(1) << "ERROR: Paint Preview failed to capture for document "
             << guid.ToString() << " with error " << status;
    std::move(callback).Run(CaptureStatus::kCaptureFailed, {});
    return;
  }
  base::UmaHistogramTimes("Browser.PaintPreview.Capture.TotalCaptureDuration",
                          base::TimeTicks::Now() - start_time);
  std::move(callback).Run(CaptureStatus::kOk, std::move(result));
}

}  // namespace paint_preview
