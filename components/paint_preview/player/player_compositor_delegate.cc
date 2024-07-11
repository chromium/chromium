// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/player_compositor_delegate.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "base/version.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/warm_compositor.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/common/version.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

namespace {

std::pair<base::UnguessableToken, std::unique_ptr<HitTester>> BuildHitTester(
    const PaintPreviewFrameProto& proto) {
  std::optional<base::UnguessableToken> embedding_token =
      base::UnguessableToken::Deserialize(proto.embedding_token_high(),
                                          proto.embedding_token_low());
  // TODO(crbug.com/40252979): Investigate whether a deserialization
  // failure can actually occur here and if it can, add a comment discussing how
  // this can happen.
  if (!embedding_token.has_value()) {
    embedding_token = base::UnguessableToken::Create();
  }
  std::pair<base::UnguessableToken, std::unique_ptr<HitTester>> out(
      embedding_token.value(), std::make_unique<HitTester>());
  out.second->Build(proto);
  return out;
}

std::unique_ptr<
    base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>>
BuildHitTesters(std::unique_ptr<PaintPreviewProto> proto) {
  TRACE_EVENT0("paint_preview", "PaintPreview BuildHitTesters");
  std::vector<std::pair<base::UnguessableToken, std::unique_ptr<HitTester>>>
      hit_testers;
  hit_testers.reserve(proto->subframes_size() + 1);
  hit_testers.push_back(BuildHitTester(proto->root_frame()));
  for (const auto& frame_proto : proto->subframes())
    hit_testers.push_back(BuildHitTester(frame_proto));

  return std::make_unique<
      base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>>(
      std::move(hit_testers));
}

paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
PrepareCompositeRequest(std::unique_ptr<CaptureResult> capture_result) {
  TRACE_EVENT0("paint_preview", "PaintPreview PrepareCompositeRequest");
  paint_preview::mojom::PaintPreviewBeginCompositeRequestPtr
      begin_composite_request =
          paint_preview::mojom::PaintPreviewBeginCompositeRequest::New();
  std::pair<RecordingMap, PaintPreviewProto> map_and_proto =
      RecordingMapFromCaptureResult(std::move(*capture_result));
  begin_composite_request->recording_map = std::move(map_and_proto.first);
  if (begin_composite_request->recording_map.empty())
    return nullptr;

  begin_composite_request->preview =
      mojo_base::ProtoWrapper(std::move(map_and_proto.second));
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
    CompositorErrorCallback compositor_error,
    base::TimeDelta timeout_duration,
    std::array<size_t, PressureLevelCount::kLevels> max_requests_map) {
  TRACE_EVENT0("paint_preview", "PlayerCompositorDelegate::Initialize");
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("paint_preview",
                                    "PlayerCompositorDelegate CreateCompositor",
                                    TRACE_ID_LOCAL(this));
  auto* memory_monitor = memory_pressure_monitor();
  // If the device is already under moderate memory pressure abort right away.
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel() >=
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
                     std::move(max_requests_map));
}

void PlayerCompositorDelegate::SetCaptureResult(
    std::unique_ptr<CaptureResult> capture_result) {
  capture_result_ = std::move(capture_result);
}

void PlayerCompositorDelegate::InitializeWithFakeServiceForTest(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    bool main_frame_mode,
    CompositorErrorCallback compositor_error,
    base::TimeDelta timeout_duration,
    std::array<size_t, PressureLevelCount::kLevels> max_requests_map,
    std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
        fake_compositor_service) {
  paint_preview_compositor_service_ = std::move(fake_compositor_service);
  paint_preview_compositor_service_->SetDisconnectHandler(
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorServiceDisconnected,
                     weak_factory_.GetWeakPtr()));

  InitializeInternal(paint_preview_service, expected_url, key, main_frame_mode,
                     std::move(compositor_error), timeout_duration,
                     std::move(max_requests_map));
}

void PlayerCompositorDelegate::InitializeInternal(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    bool main_frame_mode,
    CompositorErrorCallback compositor_error,
    base::TimeDelta timeout_duration,
    std::array<size_t, PressureLevelCount::kLevels> max_requests_map) {
  max_requests_map_ = max_requests_map;
  max_requests_ = max_requests_map_
      [base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE];
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
      FROM_HERE, base::DoNothing(),
      base::BindRepeating(&PlayerCompositorDelegate::OnMemoryPressure,
                          weak_factory_.GetWeakPtr()));
  if (!timeout_duration.is_inf() && !timeout_duration.is_zero()) {
    timeout_.Reset(
        base::BindOnce(&PlayerCompositorDelegate::OnCompositorTimeout,
                       weak_factory_.GetWeakPtr()));
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, timeout_.callback(), timeout_duration);
  }
}

int32_t PlayerCompositorDelegate::RequestBitmap(
    const std::optional<base::UnguessableToken>& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    base::OnceCallback<void(mojom::PaintPreviewCompositor::BitmapStatus,
                            const SkBitmap&)> callback,
    bool run_callback_on_default_task_runner) {
  TRACE_EVENT0("paint_preview", "PlayerCompositorDelegate::RequestBitmap");
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
                    std::move(callback).Then(base::BindOnce(
                        &PlayerCompositorDelegate::AfterBitmapRequestCallback,
                        weak_factory_.GetWeakPtr())),
                    run_callback_on_default_task_runner));
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
  if (!hit_testers_) {
    return urls;
  }

  auto it = hit_testers_->find(frame_guid);
  if (it != hit_testers_->end())
    it->second->HitTest(rect, &urls);

  return urls;
}

void PlayerCompositorDelegate::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  TRACE_EVENT1("paint_preview", "PlayerCompositorDelegate::OnMemoryPressure",
               "memory_pressure_level",
               static_cast<int>(memory_pressure_level));
  if (paint_preview_compositor_service_) {
    paint_preview_compositor_service_->OnMemoryPressure(memory_pressure_level);
  }

  DCHECK(memory_pressure_level >= 0 &&
         static_cast<size_t>(memory_pressure_level) <
             PressureLevelCount::kLevels);
  max_requests_ = max_requests_map_[memory_pressure_level];
  if (max_requests_ == 0 ||
      memory_pressure_level ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    if (paint_preview_compositor_client_)
      paint_preview_compositor_client_.reset();

    if (paint_preview_compositor_service_)
      paint_preview_compositor_service_.reset();

    if (compositor_error_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(compositor_error_),
              static_cast<int>(
                  CompositorStatus::STOPPED_DUE_TO_MEMORY_PRESSURE)));
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
  float page_scale_factor = 0.0;
  switch (status) {
    // fallthrough
    case mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess:
    case mojom::PaintPreviewCompositor::BeginCompositeStatus::kPartialSuccess:
      new_status = CompositorStatus::OK;
      page_scale_factor = page_scale_factor_;
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
      NOTREACHED_IN_MIGRATION();
  }
  OnCompositorReady(new_status, std::move(composite_response),
                    page_scale_factor, std::move(ax_tree_update_));
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
  if (!capture_result_) {
    paint_preview_service_->GetFileMixin()->GetCapturedPaintPreviewProto(
        key, std::nullopt,
        base::BindOnce(&PlayerCompositorDelegate::OnProtoAvailable,
                       weak_factory_.GetWeakPtr(), expected_url));
  } else {
    ValidateProtoAndLoadAXTree(expected_url);
  }
}

// Chrometto data suggests this function might be slow as the callback passed to
// GetCapturedPaintPreviewProto appears to block the UI thread. Nothing here
// looks to be particularly slow or blocking though...
void PlayerCompositorDelegate::OnProtoAvailable(
    const GURL& expected_url,
    PaintPreviewFileMixin::ProtoReadStatus proto_status,
    std::unique_ptr<PaintPreviewProto> proto) {
  TRACE_EVENT0("paint_preview", "PlayerCompositorDelegate::OnProtoAvailable");
  if (proto_status == PaintPreviewFileMixin::ProtoReadStatus::kExpired) {
    OnCompositorReady(CompositorStatus::CAPTURE_EXPIRED, nullptr, 0.0, nullptr);
    return;
  }

  if (proto_status == PaintPreviewFileMixin::ProtoReadStatus::kNoProto) {
    OnCompositorReady(CompositorStatus::NO_CAPTURE, nullptr, 0.0, nullptr);
    return;
  }

  if (proto_status ==
          PaintPreviewFileMixin::ProtoReadStatus::kDeserializationError ||
      !proto || !proto->IsInitialized()) {
    OnCompositorReady(CompositorStatus::PROTOBUF_DESERIALIZATION_ERROR, nullptr,
                      0.0, nullptr);
    return;
  }
  capture_result_ =
      std::make_unique<CaptureResult>(RecordingPersistence::kFileSystem);
  capture_result_->proto = std::move(*proto);

  ValidateProtoAndLoadAXTree(expected_url);
}

void PlayerCompositorDelegate::ValidateProtoAndLoadAXTree(
    const GURL& expected_url) {
  TRACE_EVENT0("paint_preview",
               "PlayerCompositorDelegate::ValidateProtoAndLoadAXTree");
  const uint32_t version = capture_result_->proto.metadata().version();
  if (version < kPaintPreviewVersion) {
    // If the version is old there was a breaking change to either;
    // - The SkPicture encoding format
    // - The storage structure
    // In either case, the new code is likely unable to deserialize the result
    // so we should early abort.
    OnCompositorReady(CompositorStatus::OLD_VERSION, nullptr, 0.0, nullptr);
    return;
  } else if (version > kPaintPreviewVersion) {
    // This shouldn't happen hence NOTREACHED(). However, in release we should
    // treat this as a new failure type to catch any possible regressions.
    OnCompositorReady(CompositorStatus::UNEXPECTED_VERSION, nullptr, 0.0,
                      nullptr);
    NOTREACHED_IN_MIGRATION();
    return;
  }

  auto proto_url = GURL(capture_result_->proto.metadata().url());
  if (expected_url != proto_url) {
    OnCompositorReady(CompositorStatus::URL_MISMATCH, nullptr, 0.0, nullptr);
    return;
  }

  if (!paint_preview_compositor_client_) {
    OnCompositorReady(CompositorStatus::COMPOSITOR_CLIENT_DISCONNECT, nullptr,
                      0.0, nullptr);
    return;
  }

  paint_preview_compositor_client_->SetRootFrameUrl(proto_url);
  root_frame_offsets_ =
      gfx::Point(capture_result_->proto.root_frame().frame_offset_x(),
                 capture_result_->proto.root_frame().frame_offset_y());

  // If the current Chrome version doesn't match the one in proto, we can't
  // use the AXTreeUpdate.
  auto chrome_version = capture_result_->proto.metadata().chrome_version();
  const auto& current_chrome_version = version_info::GetVersion();
  if (capture_result_->proto.metadata().has_chrome_version() &&
      chrome_version.major() == current_chrome_version.components()[0] &&
      chrome_version.minor() == current_chrome_version.components()[1] &&
      chrome_version.build() == current_chrome_version.components()[2] &&
      chrome_version.patch() == current_chrome_version.components()[3]) {
    paint_preview_service_->GetFileMixin()->GetAXTreeUpdate(
        key_, base::BindOnce(&PlayerCompositorDelegate::OnAXTreeUpdateAvailable,
                             weak_factory_.GetWeakPtr()));
  } else {
    PlayerCompositorDelegate::OnAXTreeUpdateAvailable(nullptr);
  }
}

void PlayerCompositorDelegate::OnAXTreeUpdateAvailable(
    std::unique_ptr<ui::AXTreeUpdate> update) {
  TRACE_EVENT0("paint_preview",
               "PlayerCompositorDelegate::OnAXTreeUpdateAvailable");
  ax_tree_update_ = std::move(update);
  proto_copy_ = std::make_unique<PaintPreviewProto>(capture_result_->proto);
  page_scale_factor_ = proto_copy_->metadata().page_scale_factor();
  if (capture_result_->persistence == RecordingPersistence::kFileSystem) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&PrepareCompositeRequest, std::move(capture_result_)),
        base::BindOnce(&PlayerCompositorDelegate::SendCompositeRequest,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  SendCompositeRequest(PrepareCompositeRequest(std::move(capture_result_)));
}

void PlayerCompositorDelegate::SendCompositeRequest(
    mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request) {
  TRACE_EVENT0("paint_preview",
               "PlayerCompositorDelegate::SendCompositeRequest");
  // TODO(crbug.com/40106234): Handle initialization errors.
  if (!begin_composite_request) {
    OnCompositorReady(CompositorStatus::INVALID_REQUEST, nullptr, 0.0, nullptr);
    return;
  }

  // It is possible the client was disconnected while loading the proto.
  if (!paint_preview_compositor_client_) {
    OnCompositorReady(CompositorStatus::COMPOSITOR_CLIENT_DISCONNECT, nullptr,
                      0.0, nullptr);
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
  if (proto_copy_) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&BuildHitTesters, std::move(proto_copy_)),
        base::BindOnce(&PlayerCompositorDelegate::OnHitTestersBuilt,
                       weak_factory_.GetWeakPtr()));
  }
  proto_copy_.reset();
}

void PlayerCompositorDelegate::OnHitTestersBuilt(
    std::unique_ptr<base::flat_map<base::UnguessableToken,
                                   std::unique_ptr<HitTester>>> hit_testers) {
  hit_testers_ = std::move(hit_testers);
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
  TRACE_EVENT0("paint_preview",
               "PlayerCompositorDelegate::ProcessBitmapRequestsFromQueue");

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

void PlayerCompositorDelegate::AfterBitmapRequestCallback() {
  active_requests_--;
  ProcessBitmapRequestsFromQueue();
}

}  // namespace paint_preview
