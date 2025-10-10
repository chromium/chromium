// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/browser/secure_embed_host.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "content/public/browser/cross_process_frame_connector_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"

namespace secure_embed {

// static
size_t SecureEmbedHost::instance_count_for_testing_ = 0;

SecureEmbedHost::SecureEmbedHost(content::RenderFrameHost* render_frame_host) {
  ++instance_count_for_testing_;
}

SecureEmbedHost::~SecureEmbedHost() {
  --instance_count_for_testing_;
}

// static
void SecureEmbedHost::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<mojom::SecureEmbedHost> receiver) {
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new SecureEmbedHost(render_frame_host)),
      std::move(receiver));
}

void SecureEmbedHost::Attach(int64_t content_id) {
  int guest_id = static_cast<int>(content_id);
  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::FromID(guest_id);

  if (!guest_handle) {
    LOG(ERROR) << "GuestContentsHandle not found for content_id: "
               << content_id;
    return;
  }

  content::WebContents* web_contents_to_attach = guest_handle->web_contents();
  if (!web_contents_to_attach) {
    LOG(ERROR) << "WebContents not found for GuestContentsHandle";
    return;
  }

  // TODO(secure-embed): Use web_contents_to_attach to complete the attachment.
  LOG(INFO) << "Successfully retrieved WebContents for content_id: "
            << content_id;
}

// static
size_t SecureEmbedHost::GetInstanceCountForTesting() {
  return instance_count_for_testing_;
}

void SecureEmbedHost::SetView(content::RenderWidgetHostViewChildFrame* view,
                              bool allow_paint_holding) {
  NOTIMPLEMENTED();
}

content::RenderWidgetHostViewBase*
SecureEmbedHost::GetParentRenderWidgetHostView() {
  NOTIMPLEMENTED();
  return nullptr;
}

content::RenderWidgetHostViewBase*
SecureEmbedHost::GetRootRenderWidgetHostView() {
  NOTIMPLEMENTED();
  return nullptr;
}

void SecureEmbedHost::RenderProcessGone() {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::FirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::SendIntrinsicSizingInfoToParent(
    blink::mojom::IntrinsicSizingInfoPtr) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool propagate) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::UpdateCursor(const ui::Cursor& cursor) {
  NOTIMPLEMENTED();
}

content::CrossProcessFrameConnectorBase::RootViewFocusState
SecureEmbedHost::HasFocus() {
  NOTIMPLEMENTED();
  return content::CrossProcessFrameConnectorBase::RootViewFocusState::kNullView;
}

void SecureEmbedHost::FocusRootView() {
  NOTIMPLEMENTED();
}

blink::mojom::PointerLockResult SecureEmbedHost::LockPointer(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

blink::mojom::PointerLockResult SecureEmbedHost::ChangePointerLock(
    bool request_unadjusted_movement) {
  NOTIMPLEMENTED();
  return blink::mojom::PointerLockResult::kUnknownError;
}

void SecureEmbedHost::UnlockPointer() {
  NOTIMPLEMENTED();
}

bool SecureEmbedHost::HasSize() const {
  NOTIMPLEMENTED();
  return false;
}

const display::ScreenInfos& SecureEmbedHost::GetScreenInfos() const {
  NOTIMPLEMENTED();
  static const base::NoDestructor<display::ScreenInfos> screen_infos;
  return *screen_infos;
}

const viz::LocalSurfaceId& SecureEmbedHost::GetLocalSurfaceId() const {
  NOTIMPLEMENTED();
  static const viz::LocalSurfaceId local_surface_id;
  return local_surface_id;
}

const blink::mojom::ViewportIntersectionState&
SecureEmbedHost::GetIntersectionState() const {
  NOTIMPLEMENTED();
  static const base::NoDestructor<blink::mojom::ViewportIntersectionState>
      intersection_state;
  return *intersection_state;
}

uint32_t SecureEmbedHost::GetCaptureSequenceNumber() const {
  NOTIMPLEMENTED();
  return 0;
}

const gfx::Rect& SecureEmbedHost::GetRectInParentViewInDip() const {
  NOTIMPLEMENTED();
  static const gfx::Rect rect;
  return rect;
}

const gfx::Size& SecureEmbedHost::GetLocalFrameSizeInDip() const {
  NOTIMPLEMENTED();
  static const gfx::Size size;
  return size;
}

const gfx::Size& SecureEmbedHost::GetLocalFrameSizeInPixels() const {
  NOTIMPLEMENTED();
  static const gfx::Size size;
  return size;
}

double SecureEmbedHost::GetCssZoomFactor() const {
  NOTIMPLEMENTED();
  return 1.0;
}

void SecureEmbedHost::EnableAutoResize(const gfx::Size& min_size,
                                       const gfx::Size& max_size) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::DisableAutoResize() {
  NOTIMPLEMENTED();
}

bool SecureEmbedHost::IsInert() const {
  NOTIMPLEMENTED();
  return false;
}

cc::TouchAction SecureEmbedHost::InheritedEffectiveTouchAction() const {
  NOTIMPLEMENTED();
  return cc::TouchAction::kAuto;
}

bool SecureEmbedHost::IsHidden() const {
  NOTIMPLEMENTED();
  return false;
}

bool SecureEmbedHost::IsThrottled() const {
  NOTIMPLEMENTED();
  return false;
}

bool SecureEmbedHost::IsSubtreeThrottled() const {
  NOTIMPLEMENTED();
  return false;
}

bool SecureEmbedHost::IsDisplayLocked() const {
  NOTIMPLEMENTED();
  return false;
}

void SecureEmbedHost::DidUpdateVisualProperties(
    const cc::RenderFrameMetadata& metadata) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::SetVisibilityForChildViews(bool visible) const {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::SetLocalFrameSize(const gfx::Size& local_frame_size) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::SetRectInParentView(
    const gfx::Rect& rect_in_parent_view) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::SetIsInert(bool inert) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::OnSetInheritedEffectiveTouchAction(cc::TouchAction) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::OnVisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::UpdateRenderThrottlingStatus(bool is_throttled,
                                                   bool subtree_throttled,
                                                   bool display_locked) {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::UpdateViewportIntersection(
    const blink::mojom::ViewportIntersectionState& intersection_state,
    const std::optional<blink::FrameVisualProperties>& visual_properties) {
  NOTIMPLEMENTED();
}

bool SecureEmbedHost::IsVisible() {
  NOTIMPLEMENTED();
  return false;
}

void SecureEmbedHost::DelegateWasShown() {
  NOTIMPLEMENTED();
}

void SecureEmbedHost::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  NOTIMPLEMENTED();
}

content::Visibility SecureEmbedHost::EmbedderVisibility() {
  NOTIMPLEMENTED();
  return content::Visibility::HIDDEN;
}

input::RenderWidgetHostViewInput* SecureEmbedHost::GetParentViewInput() {
  NOTIMPLEMENTED();
  return nullptr;
}

input::RenderWidgetHostViewInput* SecureEmbedHost::GetRootViewInput() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace secure_embed
