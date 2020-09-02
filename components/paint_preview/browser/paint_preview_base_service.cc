// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/paint_preview/browser/compositor_utils.h"
#include "components/paint_preview/browser/paint_preview_client.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

namespace {

const char kPaintPreviewDir[] = "paint_preview";

}  // namespace

PaintPreviewBaseService::PaintPreviewBaseService(
    const base::FilePath& path,
    base::StringPiece ascii_feature_name,
    std::unique_ptr<PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : policy_(std::move(policy)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::ThreadPolicy::MUST_USE_FOREGROUND})),
      file_manager_(base::MakeRefCounted<FileManager>(
          path.AppendASCII(kPaintPreviewDir).AppendASCII(ascii_feature_name),
          task_runner_)),
      is_off_the_record_(is_off_the_record) {}

PaintPreviewBaseService::~PaintPreviewBaseService() = default;

void PaintPreviewBaseService::GetCapturedPaintPreviewProto(
    const DirectoryKey& key,
    base::Optional<base::TimeDelta> expiry_horizon,
    OnReadProtoCallback on_read_proto_callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<FileManager> file_manager, const DirectoryKey& key,
             base::Optional<base::TimeDelta> expiry_horizon)
              -> std::pair<PaintPreviewBaseService::ProtoReadStatus,
                           std::unique_ptr<PaintPreviewProto>> {
            if (expiry_horizon.has_value()) {
              auto file_info = file_manager->GetInfo(key);
              if (!file_info.has_value())
                return std::make_pair(ProtoReadStatus::kNoProto, nullptr);

              if (file_info->last_modified + expiry_horizon.value() <
                  base::Time::NowFromSystemTime()) {
                return std::make_pair(ProtoReadStatus::kExpired, nullptr);
              }
            }
            auto result = file_manager->DeserializePaintPreviewProto(key);
            PaintPreviewBaseService::ProtoReadStatus status =
                ProtoReadStatus::kNoProto;
            switch (result.first) {
              case FileManager::ProtoReadStatus::kOk:
                status = ProtoReadStatus::kOk;
                break;
              case FileManager::ProtoReadStatus::kNoProto:
                status = ProtoReadStatus::kNoProto;
                break;
              case FileManager::ProtoReadStatus::kDeserializationError:
                status = ProtoReadStatus::kDeserializationError;
                break;
              default:
                NOTREACHED();
            }
            return std::make_pair(status, std::move(result.second));
          },
          file_manager_, key, expiry_horizon),
      base::BindOnce(
          [](OnReadProtoCallback callback,
             std::pair<PaintPreviewBaseService::ProtoReadStatus,
                       std::unique_ptr<PaintPreviewProto>> result) {
            std::move(callback).Run(result.first, std::move(result.second));
          },
          std::move(on_read_proto_callback)));
}

void PaintPreviewBaseService::CapturePaintPreview(
    content::WebContents* web_contents,
    const base::FilePath& root_dir,
    gfx::Rect clip_rect,
    bool capture_links,
    size_t max_per_capture_size,
    OnCapturedCallback callback) {
  CapturePaintPreview(web_contents, web_contents->GetMainFrame(), root_dir,
                      clip_rect, capture_links, max_per_capture_size,
                      std::move(callback));
}

void PaintPreviewBaseService::CapturePaintPreview(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    const base::FilePath& root_dir,
    gfx::Rect clip_rect,
    bool capture_links,
    size_t max_per_capture_size,
    OnCapturedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

  PaintPreviewClient::PaintPreviewParams params(
      RecordingPersistence::kFileSystem);
  params.root_dir = root_dir;
  params.inner.clip_rect = clip_rect;
  params.inner.is_main_frame =
      (render_frame_host == web_contents->GetMainFrame());
  params.inner.capture_links = capture_links;
  params.inner.max_capture_size = max_per_capture_size;

  // TODO(crbug/1064253): Consider moving to client so that this always happens.
  // Although, it is harder to get this right in the client due to its
  // lifecycle.
  web_contents->IncrementCapturerCount(gfx::Size(), true);

  auto start_time = base::TimeTicks::Now();
  client->CapturePaintPreview(
      params, render_frame_host,
      base::BindOnce(&PaintPreviewBaseService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetMainFrame()->GetFrameTreeNodeId(),
                     start_time, std::move(callback)));
}

void PaintPreviewBaseService::OnCaptured(
    int frame_tree_node_id,
    base::TimeTicks start_time,
    OnCapturedCallback callback,
    base::UnguessableToken guid,
    mojom::PaintPreviewStatus status,
    std::unique_ptr<CaptureResult> result) {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents)
    web_contents->DecrementCapturerCount(true);

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
