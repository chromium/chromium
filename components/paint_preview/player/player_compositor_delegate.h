// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/browser/hit_tester.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/player/compositor_status.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

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
  PlayerCompositorDelegate();
  virtual ~PlayerCompositorDelegate();

  PlayerCompositorDelegate(const PlayerCompositorDelegate&) = delete;
  PlayerCompositorDelegate& operator=(const PlayerCompositorDelegate&) = delete;

  // Initializes the compositor.
  void Initialize(PaintPreviewBaseService* paint_preview_service,
                  const GURL& url,
                  const DirectoryKey& key,
                  base::OnceCallback<void(int)> compositor_error,
                  base::TimeDelta timeout_duration);

  // Returns whether initialization has happened.
  bool IsInitialized() const { return paint_preview_service_; }

  // Overrides whether to compress the directory when the player is closed. By
  // default compression will happen.
  void SetCompressOnClose(bool compress) { compress_on_close_ = compress; }

  // Implementations should override this to handle alternative compositor ready
  // situations.
  virtual void OnCompositorReady(
      CompositorStatus compositor_status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response) {}

  // Called when there is a request for a new bitmap. When the bitmap
  // is ready, it will be passed to callback.
  void RequestBitmap(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      base::OnceCallback<void(mojom::PaintPreviewCompositor::BitmapStatus,
                              const SkBitmap&)> callback);

  // Called on touch event on a frame.
  std::vector<const GURL*> OnClick(const base::UnguessableToken& frame_guid,
                                   const gfx::Rect& rect);

  // Test methods:

  // Initializes the compositor without a real service for testing purposes.
  void InitializeWithFakeServiceForTest(
      PaintPreviewBaseService* paint_preview_service,
      const GURL& expected_url,
      const DirectoryKey& key,
      base::OnceCallback<void(int)> compositor_error,
      base::TimeDelta timeout_duration,
      std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
          fake_compositor_service);

  PaintPreviewCompositorService* GetCompositorServiceForTest() {
    return paint_preview_compositor_service_.get();
  }

  PaintPreviewCompositorClient* GetClientForTest() {
    return paint_preview_compositor_client_.get();
  }

 protected:
  base::OnceCallback<void(int)> compositor_error_;

 private:
  void InitializeInternal(PaintPreviewBaseService* paint_preview_service,
                          const GURL& expected_url,
                          const DirectoryKey& key,
                          base::OnceCallback<void(int)> compositor_error,
                          base::TimeDelta timeout_duration);

  void OnCompositorReadyStatusAdapter(
      mojom::PaintPreviewCompositor::BeginCompositeStatus status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response);

  void OnCompositorServiceDisconnected();

  void OnCompositorClientCreated(const GURL& expected_url,
                                 const DirectoryKey& key);

  void OnCompositorClientDisconnected();

  void OnCompositorTimeout();

  void OnProtoAvailable(const GURL& expected_url,
                        PaintPreviewBaseService::ProtoReadStatus proto_status,
                        std::unique_ptr<PaintPreviewProto> proto);

  void SendCompositeRequest(
      mojom::PaintPreviewBeginCompositeRequestPtr begin_composite_request);

  PaintPreviewBaseService* paint_preview_service_{nullptr};
  DirectoryKey key_;
  bool compress_on_close_{true};
  std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
      paint_preview_compositor_service_;
  std::unique_ptr<PaintPreviewCompositorClient, base::OnTaskRunnerDeleter>
      paint_preview_compositor_client_;
  base::CancelableOnceClosure timeout_;
  base::flat_map<base::UnguessableToken, std::unique_ptr<HitTester>>
      hit_testers_;
  std::unique_ptr<PaintPreviewProto> proto_;

  base::WeakPtrFactory<PlayerCompositorDelegate> weak_factory_{this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
