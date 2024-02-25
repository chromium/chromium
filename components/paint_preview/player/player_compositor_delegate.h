// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_

#include <optional>

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/hit_tester.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/player/bitmap_request.h"
#include "components/paint_preview/player/compositor_status.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"

namespace base {
class MemoryPressureMonitor;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

class SkBitmap;

namespace paint_preview {

class DirectoryKey;

// Class to facilitate a player creating and communicating with an instance of
// PaintPreviewCompositor.
class PlayerCompositorDelegate {
 public:
  enum PressureLevelCount : size_t {
    kLevels = base::MemoryPressureListener::kMaxValue + 1,
  };

  PlayerCompositorDelegate();
  virtual ~PlayerCompositorDelegate();

  PlayerCompositorDelegate(const PlayerCompositorDelegate&) = delete;
  PlayerCompositorDelegate& operator=(const PlayerCompositorDelegate&) = delete;

  // Callback used for compositor error
  using CompositorErrorCallback = base::OnceCallback<void(int32_t)>;

  // Initializes the compositor.
  void Initialize(
      PaintPreviewBaseService* paint_preview_service,
      const GURL& url,
      const DirectoryKey& key,
      bool main_frame_mode,
      CompositorErrorCallback compositor_error,
      base::TimeDelta timeout_duration,
      std::array<size_t, PressureLevelCount::kLevels> max_requests_map);

  // Returns whether initialization has happened.
  bool IsInitialized() const { return paint_preview_service_; }

  void SetCaptureResult(std::unique_ptr<CaptureResult> capture_result);

  // Overrides whether to compress the directory when the player is closed. By
  // default compression will happen.
  void SetCompressOnClose(bool compress) { compress_on_close_ = compress; }

  // Implementations should override this to handle alternative compositor ready
  // situations.
  virtual void OnCompositorReady(
      CompositorStatus compositor_status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response,
      float page_scale_factor,
      std::unique_ptr<ui::AXTreeUpdate> update) {}

  // Called when there is a request for a new bitmap. When the bitmap
  // is ready, it will be passed to callback. Returns an ID for the request.
  // Pass this ID to `CancelBitmapRequest(int32_t)` to cancel the request if it
  // hasn't already been sent.
  int32_t RequestBitmap(
      const std::optional<base::UnguessableToken>& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      base::OnceCallback<void(mojom::PaintPreviewCompositor::BitmapStatus,
                              const SkBitmap&)> callback,
      bool run_callback_on_default_task_runner = true);

  // Cancels the bitmap request associated with `request_id` if possible.
  // Returns true on success.
  bool CancelBitmapRequest(int32_t request_id);

  // Cancels all pending bitmap requests.
  void CancelAllBitmapRequests();

  // Called on touch event on a frame.
  std::vector<const GURL*> OnClick(const base::UnguessableToken& frame_guid,
                                   const gfx::Rect& rect);

  // Called when under memory pressure. The default implementation kills the
  // compositor service and client under critical pressure.
  virtual void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  gfx::Point GetRootFrameOffsets() const { return root_frame_offsets_; }

  // Test methods:

  // Initializes the compositor without a real service for testing purposes.
  void InitializeWithFakeServiceForTest(
      PaintPreviewBaseService* paint_preview_service,
      const GURL& expected_url,
      const DirectoryKey& key,
      bool main_frame_mode,
      CompositorErrorCallback compositor_error,
      base::TimeDelta timeout_duration,
      std::array<size_t, PressureLevelCount::kLevels> max_requests_map,
      std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
          fake_compositor_service);

  PaintPreviewCompositorService* GetCompositorServiceForTest() {
    return paint_preview_compositor_service_.get();
  }

  PaintPreviewCompositorClient* GetClientForTest() {
    return paint_preview_compositor_client_.get();
  }

 protected:
  CompositorErrorCallback compositor_error_;

  virtual base::MemoryPressureMonitor* memory_pressure_monitor();

 private:
  void InitializeInternal(
      PaintPreviewBaseService* paint_preview_service,
      const GURL& expected_url,
      const DirectoryKey& key,
      bool main_frame_mode,
      CompositorErrorCallback compositor_error,
      base::TimeDelta timeout_duration,
      std::array<size_t, PressureLevelCount::kLevels> max_requests_map);

  void ValidateProtoAndLoadAXTree(const GURL& expected_url);

  void OnAXTreeUpdateAvailable(std::unique_ptr<ui::AXTreeUpdate> update);

  void OnCompositorReadyStatusAdapter(
      mojom::PaintPreviewCompositor::BeginCompositeStatus status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response);

  void OnHitTestersBuilt(
      std::unique_ptr<base::flat_map<base::UnguessableToken,
                                     std::unique_ptr<HitTester>>> hit_testers);

  void OnCompositorServiceDisconnected();

  void OnCompositorClientCreated(const GURL& expected_url,
                                 const DirectoryKey& key);

  void OnCompositorClientDisconnected();

  void OnCompositorTimeout();

  void OnProtoAvailable(const GURL& expected_url,
                        PaintPreviewFileMixin::ProtoReadStatus proto_status,
                        std::unique_ptr<PaintPreviewProto> proto);

  void SendCompositeRequest(
      mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request);

  void ProcessBitmapRequestsFromQueue();
  void AfterBitmapRequestCallback();

  raw_ptr<PaintPreviewBaseService> paint_preview_service_{nullptr};
  DirectoryKey key_;
  bool compress_on_close_{true};
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_;

  std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
      paint_preview_compositor_service_;
  std::unique_ptr<PaintPreviewCompositorClient, base::OnTaskRunnerDeleter>
      paint_preview_compositor_client_;

  base::CancelableOnceClosure timeout_;
  int max_requests_{1};
  std::array<size_t, PressureLevelCount::kLevels> max_requests_map_{1, 1, 1};
  bool main_frame_mode_{false};

  std::unique_ptr<
      base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>>
      hit_testers_;
  std::unique_ptr<PaintPreviewProto> proto_copy_;
  std::unique_ptr<CaptureResult> capture_result_;
  float page_scale_factor_;
  std::unique_ptr<ui::AXTreeUpdate> ax_tree_update_;

  int active_requests_{0};
  int32_t next_request_id_{0};
  base::queue<int32_t> bitmap_request_queue_;
  std::map<int32_t, BitmapRequest> pending_bitmap_requests_;
  gfx::Point root_frame_offsets_;

  base::WeakPtrFactory<PlayerCompositorDelegate> weak_factory_{this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
