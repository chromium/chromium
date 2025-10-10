// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_
#define COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_

#include "base/component_export.h"
#include "base/notimplemented.h"
#include "components/secure_embed/common/secure_embed.mojom.h"
#include "content/public/browser/cross_process_frame_connector_base.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace secure_embed {

class COMPONENT_EXPORT(SECURE_EMBED) SecureEmbedHost
    : public mojom::SecureEmbedHost,
      public content::CrossProcessFrameConnectorBase {
 public:
  ~SecureEmbedHost() override;

  SecureEmbedHost(const SecureEmbedHost&) = delete;
  SecureEmbedHost& operator=(const SecureEmbedHost&) = delete;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedReceiver<mojom::SecureEmbedHost> receiver);

  static size_t GetInstanceCountForTesting();

  // mojom::SecureEmbedHost implementation:
  void Attach(int64_t content_id) override;

  // content::CrossProcessFrameConnectorBase:
  void SetView(content::RenderWidgetHostViewChildFrame* view,
               bool allow_paint_holding) override;
  content::RenderWidgetHostViewBase* GetParentRenderWidgetHostView() override;
  content::RenderWidgetHostViewBase* GetRootRenderWidgetHostView() override;
  void RenderProcessGone() override;
  void FirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void SendIntrinsicSizingInfoToParent(
      blink::mojom::IntrinsicSizingInfoPtr) override;
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool propagate) override;
  void UpdateCursor(const ui::Cursor& cursor) override;
  RootViewFocusState HasFocus() override;
  void FocusRootView() override;
  blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) override;
  blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement) override;
  void UnlockPointer() override;
  bool HasSize() const override;
  const display::ScreenInfos& GetScreenInfos() const override;
  const viz::LocalSurfaceId& GetLocalSurfaceId() const override;
  const blink::mojom::ViewportIntersectionState& GetIntersectionState()
      const override;
  uint32_t GetCaptureSequenceNumber() const override;
  const gfx::Rect& GetRectInParentViewInDip() const override;
  const gfx::Size& GetLocalFrameSizeInDip() const override;
  const gfx::Size& GetLocalFrameSizeInPixels() const override;
  double GetCssZoomFactor() const override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize() override;
  bool IsInert() const override;
  cc::TouchAction InheritedEffectiveTouchAction() const override;
  bool IsHidden() const override;
  bool IsThrottled() const override;
  bool IsSubtreeThrottled() const override;
  bool IsDisplayLocked() const override;
  void DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata) override;
  void SetVisibilityForChildViews(bool visible) const override;
  void SetLocalFrameSize(const gfx::Size& local_frame_size) override;
  void SetRectInParentView(const gfx::Rect& rect_in_parent_view) override;
  void SetIsInert(bool inert) override;
  void OnSetInheritedEffectiveTouchAction(cc::TouchAction) override;
  void OnVisibilityChanged(blink::mojom::FrameVisibility visibility) override;
  void UpdateRenderThrottlingStatus(bool is_throttled,
                                    bool subtree_throttled,
                                    bool display_locked) override;
  void UpdateViewportIntersection(
      const blink::mojom::ViewportIntersectionState& intersection_state,
      const std::optional<blink::FrameVisualProperties>& visual_properties)
      override;
  bool IsVisible() override;
  void DelegateWasShown() override;
  void OnSynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties) override;
  content::Visibility EmbedderVisibility() override;

  // input::ChildFrameInputHelper::Delegate:
  input::RenderWidgetHostViewInput* GetParentViewInput() override;
  input::RenderWidgetHostViewInput* GetRootViewInput() override;

 private:
  explicit SecureEmbedHost(content::RenderFrameHost* render_frame_host);

  // Count of all alive instances for testing.
  static size_t instance_count_for_testing_;
};

}  // namespace secure_embed

#endif  // COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_
