// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/player_compositor_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/warm_compositor.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/common/version.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

namespace {

std::pair<base::UnguessableToken, std::unique_ptr<HitTester>> BuildHitTester(
    const PaintPreviewFrameProto& proto) {
  std::pair<base::UnguessableToken, std::unique_ptr<HitTester>> out(
      base::UnguessableToken::Deserialize(proto.embedding_token_high(),
                                          proto.embedding_token_low()),
      std::make_unique<HitTester>());
  out.second->Build(proto);
  return out;
}

base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>
BuildHitTesters(const PaintPreviewProto& proto) {
  std::vector<std::pair<base::UnguessableToken, std::unique_ptr<HitTester>>>
      hit_testers;
  hit_testers.reserve(proto.subframes_size() + 1);
  hit_testers.push_back(BuildHitTester(proto.root_frame()));
  for (const auto& frame_proto : proto.subframes())
    hit_testers.push_back(BuildHitTester(frame_proto));

  return base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>(
      std::move(hit_testers));
}

absl::optional<base::ReadOnlySharedMemoryRegion> ToReadOnlySharedMemory(
    const paint_preview::PaintPreviewProto& proto) {
  auto region = base::WritableSharedMemoryRegion::Create(proto.ByteSizeLong());
  if (!region.IsValid())
    return absl::nullopt;

  auto mapping = region.Map();
  if (!mapping.IsValid())
    return absl::nullopt;

  proto.SerializeToArray(mapping.memory(), mapping.size());
  return base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
}

paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
PrepareCompositeRequest(const paint_preview::PaintPreviewProto& proto) {
  paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
      begin_composite_request =
          paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
  begin_composite_request->recording_map =
      RecordingMapFromPaintPreviewProto(proto);
  if (begin_composite_request->recording_map.empty())
    return nullptr;

  auto read_only_proto = ToReadOnlySharedMemory(proto);
  if (!read_only_proto) {
    DVLOG(1) << "Failed to read proto to read-only shared memory.";
    return nullptr;
  }
  begin_composite_request->proto = std::move(read_only_proto.value());
  return begin_composite_request;
}

}  // namespace

PlayerCompositorDelegate::PlayerCompositorDelegate()
    : paint_preview_compositor_service_(nullptr,
                                        base::OnTaskRunnerDeleter(nullptr)),
      paint_preview_compositor_client_(nullptr,
                                       base::OnTaskRunnerDeleter(nullptr)) {}

PlayerCompositorDelegate::~PlayerCompositorDelegate() {
  if (compress_on_close_ && paint_preview_service_) {
    paint_preview_service_->GetFileMixin()->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&FileManager::CompressDirectory),
                       paint_preview_service_->GetFileMixin()->GetFileManager(),
                       key_));
  }
}

void PlayerCompositorDelegate::Initialize(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    bool main_frame_mode,
    base::OnceCallback<void(int)> compositor_error,
    base::TimeDelta timeout_duration,
    size_t max_requests) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("paint_preview",
                                    "PlayerCompositorDelegate CreateCompositor",
                                    TRACE_ID_LOCAL(this));
  auto* memory_monitor = memory_pressure_monitor();
  // If the device is already under moderate memory pressure abort right away.
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel() >=
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(compositor_error),
                       static_cast<int>(
                           CompositorStatus::SKIPPED_DUE_TO_MEMORY_PRESSURE)));
    return;
  }

  paint_preview_compositor_service_ =
      WarmCompositor::GetInstance()->GetOrStartCompositorService(base::BindOnce(
          &PlayerCompositorDelegate::OnCompositorServiceDisconnected,
          weak_factory_.GetWeakPtr()));

  InitializeInternal(paint_preview_service, expected_url, key, main_frame_mode,
                     std::move(compositor_error), timeout_duration,
                     max_requests);
}

void PlayerCompositorDelegate::InitializeWithFakeServiceForTest(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    bool main_frame_mode,
    base::OnceCallback<void(int)> compositor_error,
    base::TimeDelta timeout_duration,
    size_t max_requests,
    std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
        fake_compositor_service) {
  paint_preview_compositor_service_ = std::move(fake_compositor_service);
  paint_preview_compositor_service_->SetDisconnectHandler(
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorServiceDisconnected,
                     weak_factory_.GetWeakPtr()));

  InitializeInternal(paint_preview_service, expected_url, key, main_frame_mode,
                     std::move(compositor_error), timeout_duration,
                     max_requests);
}

void PlayerCompositorDelegate::InitializeInternal(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    bool main_frame_mode,
    base::OnceCallback<void(int)> compositor_error,
    base::TimeDelta timeout_duration,
    size_t max_requests) {
  max_requests_ = max_requests;
  main_frame_mode_ = main_frame_mode;
  compositor_error_ = std::move(compositor_error);
  paint_preview_service_ = paint_preview_service;
  key_ = key;

  paint_preview_compositor_client_ =
      paint_preview_compositor_service_->CreateCompositor(
          base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientCreated,
                         weak_factory_.GetWeakPtr(), expected_url, key));
  paint_preview_compositor_client_->SetDisconnectHandler(
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorClientDisconnected,
                     weak_factory_.GetWeakPtr()));

  memory_pressure_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&PlayerCompositorDelegate::OnMemoryPressure,
                          weak_factory_.GetWeakPtr()));
  if (!timeout_duration.is_inf() && !timeout_duration.is_zero()) {
    timeout_.Reset(
        base::BindOnce(&PlayerCompositorDelegate::OnCompositorTimeout,
                       weak_factory_.GetWeakPtr()));
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout_.callback(), timeout_duration);
  }
}

int32_t PlayerCompositorDelegate::RequestBitmap(
    const absl::optional<base::UnguessableToken>& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    base::OnceCallback<void(mojom::PaintPreviewCompositor::BitmapStatus,
                            const SkBitmap&)> callback) {
  DCHECK(IsInitialized());
  DCHECK((main_frame_mode_ && !frame_guid.has_value()) ||
         (!main_frame_mode_ && frame_guid.has_value()));
  const int32_t request_id = next_request_id_;
  next_request_id_++;
  if (!paint_preview_compositor_client_) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame, SkBitmap());
    return request_id;
  }

  bitmap_request_queue_.push(request_id);
  pending_bitmap_requests_.emplace(
      request_id,
      BitmapRequest(frame_guid, clip_rect, scale_factor,
                    base::BindOnce(
                        &PlayerCompositorDelegate::BitmapRequestCallbackAdapter,
                        weak_factory_.GetWeakPtr(), std::move(callback))));
  ProcessBitmapRequestsFromQueue();
  return request_id;
}

bool PlayerCompositorDelegate::CancelBitmapRequest(int32_t request_id) {
  auto it = pending_bitmap_requests_.find(request_id);
  if (it == pending_bitmap_requests_.end())
    return false;

  pending_bitmap_requests_.erase(it);
  return true;
}

void PlayerCompositorDelegate::CancelAllBitmapRequests() {
  while (bitmap_request_queue_.size())
    bitmap_request_queue_.pop();

  pending_bitmap_requests_.clear();
}

std::vector<const GURL*> PlayerCompositorDelegate::OnClick(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& rect) {
  DCHECK(IsInitialized());
  std::vector<const GURL*> urls;
  auto it = hit_testers_.find(frame_guid);
  if (it != hit_testers_.end())
    it->second->HitTest(rect, &urls);

  return urls;
}

void PlayerCompositorDelegate::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    if (paint_preview_compositor_client_)
      paint_preview_compositor_client_.reset();

    if (paint_preview_compositor_service_)
      paint_preview_compositor_service_.reset();

    if (compositor_error_) {
      std::move(compositor_error_)
          .Run(static_cast<int>(
              CompositorStatus::STOPPED_DUE_TO_MEMORY_PRESSURE));
    }
  }
}

base::MemoryPressureMonitor*
PlayerCompositorDelegate::memory_pressure_monitor() {
  return base::MemoryPressureMonitor::Get();
}

void PlayerCompositorDelegate::OnCompositorReadyStatusAdapter(
    mojom::PaintPreviewCompositor::BeginCompositeStatus status,
    mojom::PaintPreviewBeginCompositeResponsePtr composite_response) {
  timeout_.Cancel();
  CompositorStatus new_status;
  switch (status) {
    // fallthrough
    case mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess:
    case mojom::PaintPreviewCompositor::BeginCompositeStatus::kPartialSuccess:
      new_status = CompositorStatus::OK;
      break;
    case mojom::PaintPreviewCompositor::BeginCompositeStatus::
        kDeserializingFailure:
      new_status = CompositorStatus::COMPOSITOR_DESERIALIZATION_ERROR;
      break;
    case mojom::PaintPreviewCompositor::BeginCompositeStatus::
        kCompositingFailure:
      new_status = CompositorStatus::INVALID_ROOT_FRAME_SKP;
      break;
    default:
      NOTREACHED();
  }
  OnCompositorReady(new_status, std::move(composite_response),
                    std::move(ax_tree_update_));
}

void PlayerCompositorDelegate::OnCompositorServiceDisconnected() {
  DLOG(ERROR) << "Compositor service disconnected.";
  if (compositor_error_) {
    std::move(compositor_error_)
        .Run(static_cast<int>(CompositorStatus::COMPOSITOR_SERVICE_DISCONNECT));
  }
}

void PlayerCompositorDelegate::OnCompositorClientCreated(
    const GURL& expected_url,
    const DirectoryKey& key) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("paint_preview",
                                  "PlayerCompositorDelegate CreateCompositor",
                                  TRACE_ID_LOCAL(this));
  if (!proto_) {
    paint_preview_service_->GetFileMixin()->GetCapturedPaintPreviewProto(
        key, absl::nullopt,
        base::BindOnce(&PlayerCompositorDelegate::OnProtoAvailable,
                       weak_factory_.GetWeakPtr(), expected_url));
  } else {
    OnProtoAvailable(expected_url, PaintPreviewFileMixin::ProtoReadStatus::kOk,
                     std::move(proto_));
  }
}

void PlayerCompositorDelegate::OnProtoAvailable(
    const GURL& expected_url,
    PaintPreviewFileMixin::ProtoReadStatus proto_status,
    std::unique_ptr<PaintPreviewProto> proto) {
  if (proto_status == PaintPreviewFileMixin::ProtoReadStatus::kExpired) {
    OnCompositorReady(CompositorStatus::CAPTURE_EXPIRED, nullptr, nullptr);
    return;
  }

  if (proto_status == PaintPreviewFileMixin::ProtoReadStatus::kNoProto) {
    OnCompositorReady(CompositorStatus::NO_CAPTURE, nullptr, nullptr);
    return;
  }

  if (proto_status ==
          PaintPreviewFileMixin::ProtoReadStatus::kDeserializationError ||
      !proto || !proto->IsInitialized()) {
    OnCompositorReady(CompositorStatus::PROTOBUF_DESERIALIZATION_ERROR, nullptr,
                      nullptr);
    return;
  }

  const uint32_t version = proto->metadata().version();
  if (version < kPaintPreviewVersion) {
    // If the version is old there was a breaking change to either;
    // - The SkPicture encoding format
    // - The storage structure
    // In either case, the new code is likely unable to deserialize the result
    // so we should early abort.
    OnCompositorReady(CompositorStatus::OLD_VERSION, nullptr, nullptr);
    return;
  } else if (version > kPaintPreviewVersion) {
    // This shouldn't happen hence NOTREACHED(). However, in release we should
    // treat this as a new failure type to catch any possible regressions.
    OnCompositorReady(CompositorStatus::UNEXPECTED_VERSION, nullptr, nullptr);
    NOTREACHED();
    return;
  }

  auto proto_url = GURL(proto->metadata().url());
  if (expected_url != proto_url) {
    OnCompositorReady(CompositorStatus::URL_MISMATCH, nullptr, nullptr);
    return;
  }

  if (!paint_preview_compositor_client_) {
    OnCompositorReady(CompositorStatus::COMPOSITOR_CLIENT_DISCONNECT, nullptr,
                      nullptr);
    return;
  }

  paint_preview_compositor_client_->SetRootFrameUrl(proto_url);
  proto_ = std::move(proto);

  // If the current Chrome version doesn't match the one in proto, we can't
  // use the AXTreeUpdate.
  auto chromeVersion = proto_->metadata().chrome_version();
  if (proto_->metadata().has_chrome_version() &&
      chromeVersion.major() == CHROME_VERSION_MAJOR &&
      chromeVersion.minor() == CHROME_VERSION_MINOR &&
      chromeVersion.build() == CHROME_VERSION_BUILD &&
      chromeVersion.patch() == CHROME_VERSION_PATCH) {
    paint_preview_service_->GetFileMixin()->GetAXTreeUpdate(
        key_, base::BindOnce(&PlayerCompositorDelegate::OnAXTreeUpdateAvailable,
                             weak_factory_.GetWeakPtr()));
  } else {
    PlayerCompositorDelegate::OnAXTreeUpdateAvailable(nullptr);
  }
}

void PlayerCompositorDelegate::OnAXTreeUpdateAvailable(
    std::unique_ptr<ui::AXTreeUpdate> update) {
  ax_tree_update_ = std::move(update);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&PrepareCompositeRequest, *proto_),
      base::BindOnce(&PlayerCompositorDelegate::SendCompositeRequest,
                     weak_factory_.GetWeakPtr()));
}

void PlayerCompositorDelegate::SendCompositeRequest(
    mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request) {
  // TODO(crbug.com/1021590): Handle initialization errors.
  if (!begin_composite_request) {
    OnCompositorReady(CompositorStatus::INVALID_REQUEST, nullptr, nullptr);
    return;
  }

  // It is possible the client was disconnected while loading the proto.
  if (!paint_preview_compositor_client_) {
    OnCompositorReady(CompositorStatus::COMPOSITOR_CLIENT_DISCONNECT, nullptr,
                      nullptr);
    return;
  }

  if (main_frame_mode_) {
    paint_preview_compositor_client_->BeginMainFrameComposite(
        std::move(begin_composite_request),
        base::BindOnce(
            &PlayerCompositorDelegate::OnCompositorReadyStatusAdapter,
            weak_factory_.GetWeakPtr()));

  } else {
    paint_preview_compositor_client_->BeginSeparatedFrameComposite(
        std::move(begin_composite_request),
        base::BindOnce(
            &PlayerCompositorDelegate::OnCompositorReadyStatusAdapter,
            weak_factory_.GetWeakPtr()));
  }

  // Defer building hit testers so it happens in parallel with preparing the
  // compositor.
  hit_testers_ = BuildHitTesters(*proto_);
  proto_.reset();
}

void PlayerCompositorDelegate::OnCompositorClientDisconnected() {
  DLOG(ERROR) << "Compositor client disconnected.";
  if (compositor_error_) {
    std::move(compositor_error_)
        .Run(static_cast<int>(CompositorStatus::COMPOSITOR_CLIENT_DISCONNECT));
  }
}

void PlayerCompositorDelegate::OnCompositorTimeout() {
  DLOG(ERROR) << "Compositor process startup timed out.";
  if (compositor_error_) {
    std::move(compositor_error_)
        .Run(static_cast<int>(CompositorStatus::TIMED_OUT));
  }
}

void PlayerCompositorDelegate::ProcessBitmapRequestsFromQueue() {
  while (active_requests_ < max_requests_ && bitmap_request_queue_.size()) {
    int request_id = bitmap_request_queue_.front();
    bitmap_request_queue_.pop();

    auto it = pending_bitmap_requests_.find(request_id);
    if (it == pending_bitmap_requests_.end())
      continue;

    BitmapRequest& request = it->second;
    active_requests_++;
    // If the client disconnects mid request, just give up as we should be
    // exiting.
    if (!paint_preview_compositor_client_)
      return;

    if (request.frame_guid.has_value()) {
      paint_preview_compositor_client_->BitmapForSeparatedFrame(
          request.frame_guid.value(), request.clip_rect, request.scale_factor,
          std::move(request.callback));
    } else {
      paint_preview_compositor_client_->BitmapForMainFrame(
          request.clip_rect, request.scale_factor, std::move(request.callback));
    }
    pending_bitmap_requests_.erase(it);
  }
}

void PlayerCompositorDelegate::BitmapRequestCallbackAdapter(
    base::OnceCallback<void(mojom::PaintPreviewCompositor::BitmapStatus,
                            const SkBitmap&)> callback,
    mojom::PaintPreviewCompositor::BitmapStatus status,
    const SkBitmap& bitmap) {
  std::move(callback).Run(status, bitmap);

  active_requests_--;
  ProcessBitmapRequestsFromQueue();
}

}  // namespace paint_preview
