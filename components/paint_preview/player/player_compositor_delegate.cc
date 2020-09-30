// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/player_compositor_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/optional.h"
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

base::Optional<base::ReadOnlySharedMemoryRegion> ToReadOnlySharedMemory(
    const paint_preview::PaintPreviewProto& proto) {
  auto region = base::WritableSharedMemoryRegion::Create(proto.ByteSizeLong());
  if (!region.IsValid())
    return base::nullopt;

  auto mapping = region.Map();
  if (!mapping.IsValid())
    return base::nullopt;

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
  if (compress_on_close_) {
    paint_preview_service_->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&FileManager::CompressDirectory),
                       paint_preview_service_->GetFileManager(), key_));
  }
}

void PlayerCompositorDelegate::Initialize(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    base::OnceCallback<void(int)> compositor_error,
    base::TimeDelta timeout_duration) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("paint_preview",
                                    "PlayerCompositorDelegate CreateCompositor",
                                    TRACE_ID_LOCAL(this));
  paint_preview_compositor_service_ =
      WarmCompositor::GetInstance()->GetOrStartCompositorService(base::BindOnce(
          &PlayerCompositorDelegate::OnCompositorServiceDisconnected,
          weak_factory_.GetWeakPtr()));

  InitializeInternal(paint_preview_service, expected_url, key,
                     std::move(compositor_error), timeout_duration);
}

void PlayerCompositorDelegate::InitializeWithFakeServiceForTest(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    base::OnceCallback<void(int)> compositor_error,
    base::TimeDelta timeout_duration,
    std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
        fake_compositor_service) {
  paint_preview_compositor_service_ = std::move(fake_compositor_service);
  paint_preview_compositor_service_->SetDisconnectHandler(
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorServiceDisconnected,
                     weak_factory_.GetWeakPtr()));

  InitializeInternal(paint_preview_service, expected_url, key,
                     std::move(compositor_error), timeout_duration);
}

void PlayerCompositorDelegate::InitializeInternal(
    PaintPreviewBaseService* paint_preview_service,
    const GURL& expected_url,
    const DirectoryKey& key,
    base::OnceCallback<void(int)> compositor_error,
    base::TimeDelta timeout_duration) {
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

  if (!timeout_duration.is_inf() && !timeout_duration.is_zero()) {
    timeout_.Reset(
        base::BindOnce(&PlayerCompositorDelegate::OnCompositorTimeout,
                       weak_factory_.GetWeakPtr()));
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout_.callback(), timeout_duration);
  }
}

void PlayerCompositorDelegate::RequestBitmap(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    base::OnceCallback<void(mojom::PaintPreviewCompositor::BitmapStatus,
                            const SkBitmap&)> callback) {
  DCHECK(IsInitialized());
  if (!paint_preview_compositor_client_) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame, SkBitmap());
    return;
  }

  paint_preview_compositor_client_->BitmapForSeparatedFrame(
      frame_guid, clip_rect, scale_factor, std::move(callback));
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
  OnCompositorReady(new_status, std::move(composite_response));
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
  paint_preview_service_->GetCapturedPaintPreviewProto(
      key, base::nullopt,
      base::BindOnce(&PlayerCompositorDelegate::OnProtoAvailable,
                     weak_factory_.GetWeakPtr(), expected_url));
}

void PlayerCompositorDelegate::OnProtoAvailable(
    const GURL& expected_url,
    PaintPreviewBaseService::ProtoReadStatus proto_status,
    std::unique_ptr<PaintPreviewProto> proto) {
  if (proto_status == PaintPreviewBaseService::ProtoReadStatus::kExpired) {
    OnCompositorReady(CompositorStatus::CAPTURE_EXPIRED, nullptr);
    return;
  }

  if (proto_status == PaintPreviewBaseService::ProtoReadStatus::kNoProto) {
    OnCompositorReady(CompositorStatus::NO_CAPTURE, nullptr);
    return;
  }

  if (proto_status ==
          PaintPreviewBaseService::ProtoReadStatus::kDeserializationError ||
      !proto || !proto->IsInitialized()) {
    OnCompositorReady(CompositorStatus::PROTOBUF_DESERIALIZATION_ERROR,
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
    OnCompositorReady(CompositorStatus::OLD_VERSION, nullptr);
    return;
  } else if (version > kPaintPreviewVersion) {
    // This shouldn't happen hence NOTREACHED(). However, in release we should
    // treat this as a new failure type to catch any possible regressions.
    OnCompositorReady(CompositorStatus::UNEXPECTED_VERSION, nullptr);
    NOTREACHED();
    return;
  }

  auto proto_url = GURL(proto->metadata().url());
  if (expected_url != proto_url) {
    OnCompositorReady(CompositorStatus::URL_MISMATCH, nullptr);
    return;
  }

  if (!paint_preview_compositor_client_) {
    OnCompositorReady(CompositorStatus::COMPOSITOR_CLIENT_DISCONNECT, nullptr);
    return;
  }

  paint_preview_compositor_client_->SetRootFrameUrl(proto_url);

  proto_ = std::move(proto);
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
    OnCompositorReady(CompositorStatus::INVALID_REQUEST, nullptr);
    return;
  }

  paint_preview_compositor_client_->BeginSeparatedFrameComposite(
      std::move(begin_composite_request),
      base::BindOnce(&PlayerCompositorDelegate::OnCompositorReadyStatusAdapter,
                     weak_factory_.GetWeakPtr()));

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

}  // namespace paint_preview
