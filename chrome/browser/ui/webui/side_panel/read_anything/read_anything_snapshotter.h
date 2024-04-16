// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SNAPSHOTTER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SNAPSHOTTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "content/public/browser/web_contents.h"

// A class that helps Reading Mode (i.e. ReadAnything) to capture a full-page
// screenshot of an active WebContents. This screenshot functionality will be
// enabled only by a command-line switch and should not be used for user-facing
// features.
class ReadAnythingSnapshotter : public paint_preview::PaintPreviewBaseService {
 public:
  ReadAnythingSnapshotter();
  ReadAnythingSnapshotter(const ReadAnythingSnapshotter&) = delete;
  ReadAnythingSnapshotter& operator=(const ReadAnythingSnapshotter&) = delete;
  ~ReadAnythingSnapshotter() override;

  void RequestSnapshot(const raw_ptr<content::WebContents> web_contents);

 private:
  void OnSnapshotCaptured(
      paint_preview::PaintPreviewBaseService::CaptureStatus status,
      std::unique_ptr<paint_preview::CaptureResult> result);
  void OnCompositorServiceDisconnected();
  paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
  PrepareCompositeRequest(
      std::unique_ptr<paint_preview::CaptureResult> capture_result);
  void SendCompositeRequest(
      paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
          begin_composite_request);
  void RequestBitmapForMainFrame();
  void OnCompositeFinished(
      paint_preview::mojom::PaintPreviewCompositor::BeginCompositeStatus status,
      paint_preview::mojom::PaintPreviewBeginCompositeResponsePtr response);
  void OnBitmapReceived(
      paint_preview::mojom::PaintPreviewCompositor::BitmapStatus status,
      const SkBitmap& bitmap);

  std::unique_ptr<paint_preview::PaintPreviewCompositorService,
                  base::OnTaskRunnerDeleter>
      paint_preview_compositor_service_;
  std::unique_ptr<paint_preview::PaintPreviewCompositorClient,
                  base::OnTaskRunnerDeleter>
      paint_preview_compositor_client_;

  base::WeakPtrFactory<ReadAnythingSnapshotter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SNAPSHOTTER_H_
