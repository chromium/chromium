// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/exo/buffer.h"
#include "components/exo/pointer.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/path.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::Surface*);

namespace exo {
namespace {

// A property key containing the surface that is associated with
// window. If unset, no surface is associated with window.
DEFINE_UI_CLASS_PROPERTY_KEY(Surface*, kSurfaceKey, nullptr);

// A property key to store whether the surface should only consume
// stylus input events.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kStylusOnlyKey, false);

// Helper function that returns an iterator to the first entry in |list|
// with |key|.
template <typename T, typename U>
typename T::iterator FindListEntry(T& list, U key) {
  return std::find_if(list.begin(), list.end(),
                      [key](const typename T::value_type& entry) {
                        return entry.first == key;
                      });
}

// Helper function that returns true if |list| contains an entry with |key|.
template <typename T, typename U>
bool ListContainsEntry(T& list, U key) {
  return FindListEntry(list, key) != list.end();
}

// Helper function that returns true if |format| may have an alpha channel.
// Note: False positives are allowed but false negatives are not.
bool FormatHasAlpha(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      return false;
    default:
      return true;
  }
}

// Helper function that returns |size| after adjusting for |transform|.
gfx::Size ToTransformedSize(const gfx::Size& size, Transform transform) {
  switch (transform) {
    case Transform::NORMAL:
    case Transform::ROTATE_180:
      return size;
    case Transform::ROTATE_90:
    case Transform::ROTATE_270:
      return gfx::Size(size.height(), size.width());
  }

  NOTREACHED();
}

class CustomWindowDelegate : public aura::WindowDelegate {
 public:
  explicit CustomWindowDelegate(Surface* surface) : surface_(surface) {}
  ~CustomWindowDelegate() override {}

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface_->window());
    if (widget)
      return widget->GetNativeWindow()->GetCursor(point /* not used */);
    return ui::CursorType::kNull;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return true; }
  void GetHitTestMask(gfx::Path* mask) const override {
    surface_->GetHitTestMask(mask);
  }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Propagates the key event upto the top-level views Widget so that we can
    // trigger proper events in the views/ash level there. Event handling for
    // Surfaces is done in a post event handler in keyboard.cc.
    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface_->window());
    if (widget)
      widget->OnKeyEvent(event);
  }

 private:
  Surface* const surface_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowDelegate);
};

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  CustomWindowTargeter() {}
  ~CustomWindowTargeter() override {}

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    Surface* surface = Surface::AsSurface(window);
    if (!surface || !surface->IsInputEnabled(surface))
      return false;

    gfx::Point local_point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);
    return surface->HitTest(local_point);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Surface, public:

Surface::Surface()
    : window_(std::make_unique<aura::Window>(new CustomWindowDelegate(this),
                                             aura::client::WINDOW_TYPE_CONTROL,
                                             WMHelper::GetInstance()->env())) {
  window_->SetName("ExoSurface");
  window_->SetProperty(kSurfaceKey, this);
  window_->Init(ui::LAYER_NOT_DRAWN);
  window_->SetEventTargeter(std::make_unique<CustomWindowTargeter>());
  window_->set_owned_by_parent(false);
  WMHelper::GetInstance()->SetDragDropDelegate(window_.get());
}
Surface::~Surface() {
  for (SurfaceObserver& observer : observers_)
    observer.OnSurfaceDestroying(this);

  // Call all frame callbacks with a null frame time to indicate that they
  // have been cancelled.
  frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);
  for (const auto& frame_callback : frame_callbacks_)
    frame_callback.Run(base::TimeTicks());

  // Call all presentation callbacks with a null presentation time to indicate
  // that they have been cancelled.
  presentation_callbacks_.splice(presentation_callbacks_.end(),
                                 pending_presentation_callbacks_);
  for (const auto& presentation_callback : presentation_callbacks_)
    presentation_callback.Run(gfx::PresentationFeedback());

  WMHelper::GetInstance()->ResetDragDropDelegate(window_.get());
}

// static
Surface* Surface::AsSurface(const aura::Window* window) {
  return window->GetProperty(kSurfaceKey);
}

void Surface::Attach(Buffer* buffer) {
  TRACE_EVENT1("exo", "Surface::Attach", "buffer",
               buffer ? buffer->GetSize().ToString() : "null");

  has_pending_contents_ = true;
  pending_buffer_.Reset(buffer ? buffer->AsWeakPtr() : base::WeakPtr<Buffer>());
}

void Surface::Damage(const gfx::Rect& damage) {
  TRACE_EVENT1("exo", "Surface::Damage", "damage", damage.ToString());

  pending_damage_.Union(damage);
}

void Surface::RequestFrameCallback(const FrameCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestFrameCallback");

  pending_frame_callbacks_.push_back(callback);
}

void Surface::RequestPresentationCallback(
    const PresentationCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestPresentationCallback");

  pending_presentation_callbacks_.push_back(callback);
}

void Surface::SetOpaqueRegion(const cc::Region& region) {
  TRACE_EVENT1("exo", "Surface::SetOpaqueRegion", "region", region.ToString());

  pending_state_.opaque_region = region;
}

void Surface::SetInputRegion(const cc::Region& region) {
  TRACE_EVENT1("exo", "Surface::SetInputRegion", "region", region.ToString());

  pending_state_.input_region = region;
}

void Surface::ResetInputRegion() {
  TRACE_EVENT0("exo", "Surface::ResetInputRegion");

  pending_state_.input_region = base::nullopt;
}

void Surface::SetInputOutset(int outset) {
  TRACE_EVENT1("exo", "Surface::SetInputOutset", "outset", outset);

  pending_state_.input_outset = outset;
}

void Surface::SetBufferScale(float scale) {
  TRACE_EVENT1("exo", "Surface::SetBufferScale", "scale", scale);

  pending_state_.buffer_scale = scale;
}

void Surface::SetBufferTransform(Transform transform) {
  TRACE_EVENT1("exo", "Surface::SetBufferTransform", "transform",
               static_cast<int>(transform));

  pending_state_.buffer_transform = transform;
}

void Surface::AddSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::AddSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  DCHECK(!sub_surface->window()->parent());
  DCHECK(!sub_surface->window()->IsVisible());
  sub_surface->window()->SetBounds(
      gfx::Rect(sub_surface->window()->bounds().size()));
  window_->AddChild(sub_surface->window());

  DCHECK(!ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.push_back(std::make_pair(sub_surface, gfx::Point()));
  sub_surfaces_.push_back(std::make_pair(sub_surface, gfx::Point()));
  sub_surfaces_changed_ = true;
}

void Surface::RemoveSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::RemoveSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  window_->RemoveChild(sub_surface->window());
  if (sub_surface->window()->IsVisible())
    sub_surface->window()->Hide();

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.erase(
      FindListEntry(pending_sub_surfaces_, sub_surface));

  DCHECK(ListContainsEntry(sub_surfaces_, sub_surface));
  auto it = FindListEntry(sub_surfaces_, sub_surface);
  sub_surfaces_.erase(it);
  // Force recreating resources when the surface is added to a tree again.
  sub_surface->SurfaceHierarchyResourcesLost();
  sub_surfaces_changed_ = true;
}

void Surface::SetSubSurfacePosition(Surface* sub_surface,
                                    const gfx::Point& position) {
  TRACE_EVENT2("exo", "Surface::SetSubSurfacePosition", "sub_surface",
               sub_surface->AsTracedValue(), "position", position.ToString());

  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  DCHECK(it != pending_sub_surfaces_.end());
  if (it->second == position)
    return;
  it->second = position;
  sub_surfaces_changed_ = true;
}

void Surface::PlaceSubSurfaceAbove(Surface* sub_surface, Surface* reference) {
  TRACE_EVENT2("exo", "Surface::PlaceSubSurfaceAbove", "sub_surface",
               sub_surface->AsTracedValue(), "reference",
               reference->AsTracedValue());

  if (sub_surface == reference) {
    DLOG(WARNING) << "Client tried to place sub-surface above itself";
    return;
  }

  auto position_it = pending_sub_surfaces_.begin();
  if (reference != this) {
    position_it = FindListEntry(pending_sub_surfaces_, reference);
    if (position_it == pending_sub_surfaces_.end()) {
      DLOG(WARNING) << "Client tried to place sub-surface above a reference "
                       "surface that is neither a parent nor a sibling";
      return;
    }

    // Advance iterator to have |position_it| point to the sibling surface
    // above |reference|.
    ++position_it;
  }

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  if (it == position_it)
    return;
  pending_sub_surfaces_.splice(position_it, pending_sub_surfaces_, it);
  sub_surfaces_changed_ = true;
}

void Surface::PlaceSubSurfaceBelow(Surface* sub_surface, Surface* sibling) {
  TRACE_EVENT2("exo", "Surface::PlaceSubSurfaceBelow", "sub_surface",
               sub_surface->AsTracedValue(), "sibling",
               sibling->AsTracedValue());

  if (sub_surface == sibling) {
    DLOG(WARNING) << "Client tried to place sub-surface below itself";
    return;
  }

  auto sibling_it = FindListEntry(pending_sub_surfaces_, sibling);
  if (sibling_it == pending_sub_surfaces_.end()) {
    DLOG(WARNING) << "Client tried to place sub-surface below a surface that "
                     "is not a sibling";
    return;
  }

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  if (it == sibling_it)
    return;
  pending_sub_surfaces_.splice(sibling_it, pending_sub_surfaces_, it);
  sub_surfaces_changed_ = true;
}

void Surface::OnSubSurfaceCommit() {
  if (delegate_)
    delegate_->OnSurfaceCommit();
}

void Surface::SetViewport(const gfx::Size& viewport) {
  TRACE_EVENT1("exo", "Surface::SetViewport", "viewport", viewport.ToString());

  pending_state_.viewport = viewport;
}

void Surface::SetCrop(const gfx::RectF& crop) {
  TRACE_EVENT1("exo", "Surface::SetCrop", "crop", crop.ToString());

  pending_state_.crop = crop;
}

void Surface::SetOnlyVisibleOnSecureOutput(bool only_visible_on_secure_output) {
  TRACE_EVENT1("exo", "Surface::SetOnlyVisibleOnSecureOutput",
               "only_visible_on_secure_output", only_visible_on_secure_output);

  pending_state_.only_visible_on_secure_output = only_visible_on_secure_output;
}

void Surface::SetBlendMode(SkBlendMode blend_mode) {
  TRACE_EVENT1("exo", "Surface::SetBlendMode", "blend_mode",
               static_cast<int>(blend_mode));

  pending_state_.blend_mode = blend_mode;
}

void Surface::SetAlpha(float alpha) {
  TRACE_EVENT1("exo", "Surface::SetAlpha", "alpha", alpha);

  pending_state_.alpha = alpha;
}

void Surface::SetFrame(SurfaceFrameType type) {
  TRACE_EVENT1("exo", "Surface::SetFrame", "type", static_cast<uint32_t>(type));

  if (delegate_)
    delegate_->OnSetFrame(type);
}

void Surface::SetFrameColors(SkColor active_color, SkColor inactive_color) {
  TRACE_EVENT2("exo", "Surface::SetFrameColors", "active_color", active_color,
               "inactive_color", inactive_color);

  if (delegate_)
    delegate_->OnSetFrameColors(active_color, inactive_color);
}

void Surface::SetStartupId(const char* startup_id) {
  TRACE_EVENT1("exo", "Surface::SetStartupId", "startup_id", startup_id);

  if (delegate_)
    delegate_->OnSetStartupId(startup_id);
}

void Surface::SetApplicationId(const char* application_id) {
  TRACE_EVENT1("exo", "Surface::SetApplicationId", "application_id",
               application_id);

  if (delegate_)
    delegate_->OnSetApplicationId(application_id);
}

void Surface::SetParent(Surface* parent, const gfx::Point& position) {
  TRACE_EVENT2("exo", "Surface::SetParent", "parent", !!parent, "position",
               position.ToString());

  if (delegate_)
    delegate_->OnSetParent(parent, position);
}

void Surface::Commit() {
  TRACE_EVENT0("exo", "Surface::Commit");

  needs_commit_surface_ = true;
  if (delegate_)
    delegate_->OnSurfaceCommit();
  else
    CommitSurfaceHierarchy(false);
}

void Surface::CommitSurfaceHierarchy(bool synchronized) {
  if (needs_commit_surface_ && (synchronized || !IsSynchronized())) {
    needs_commit_surface_ = false;
    synchronized = true;

    // TODO(penghuang): Make the damage more precise for sub surface changes.
    // https://crbug.com/779704
    bool needs_full_damage =
        sub_surfaces_changed_ ||
        pending_state_.opaque_region != state_.opaque_region ||
        pending_state_.buffer_scale != state_.buffer_scale ||
        pending_state_.buffer_transform != state_.buffer_transform ||
        pending_state_.viewport != state_.viewport ||
        pending_state_.crop != state_.crop ||
        pending_state_.only_visible_on_secure_output !=
            state_.only_visible_on_secure_output ||
        pending_state_.blend_mode != state_.blend_mode ||
        pending_state_.alpha != state_.alpha;

    bool needs_update_buffer_transform =
        pending_state_.buffer_scale != state_.buffer_scale ||
        pending_state_.buffer_transform != state_.buffer_transform;

    // If the current state is fully transparent, the last submitted frame will
    // not include the TextureDrawQuad for the resource, so the resource might
    // have been released and needs to be updated again.
    if (!state_.alpha && pending_state_.alpha)
      needs_update_resource_ = true;

    state_ = pending_state_;
    pending_state_.only_visible_on_secure_output = false;

    window_->SetEventTargetingPolicy(
        (state_.input_region.has_value() && state_.input_region->IsEmpty())
            ? ws::mojom::EventTargetingPolicy::DESCENDANTS_ONLY
            : ws::mojom::EventTargetingPolicy::TARGET_AND_DESCENDANTS);

    // We update contents if Attach() has been called since last commit.
    if (has_pending_contents_) {
      has_pending_contents_ = false;
      current_buffer_ = std::move(pending_buffer_);
      if (state_.alpha)
        needs_update_resource_ = true;
    }

    if (needs_update_buffer_transform)
      UpdateBufferTransform();

    // Move pending frame callbacks to the end of |frame_callbacks_|.
    frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);

    // Move pending presentation callbacks to the end of
    // |presentation_callbacks_|.
    presentation_callbacks_.splice(presentation_callbacks_.end(),
                                   pending_presentation_callbacks_);

    UpdateContentSize();

    // Synchronize window hierarchy. This will position and update the stacking
    // order of all sub-surfaces after committing all pending state of
    // sub-surface descendants.
    if (sub_surfaces_changed_) {
      sub_surfaces_.clear();
      aura::Window* stacking_target = nullptr;
      for (const auto& sub_surface_entry : pending_sub_surfaces_) {
        Surface* sub_surface = sub_surface_entry.first;
        sub_surfaces_.push_back(sub_surface_entry);
        // Move sub-surface to its new position in the stack.
        if (stacking_target)
          window_->StackChildAbove(sub_surface->window(), stacking_target);

        // Stack next sub-surface above this sub-surface.
        stacking_target = sub_surface->window();

        // Update sub-surface position relative to surface origin.
        sub_surface->window()->SetBounds(gfx::Rect(
            sub_surface_entry.second, sub_surface->window()->bounds().size()));
      }
      sub_surfaces_changed_ = false;
    }

    gfx::Rect output_rect(content_size_);
    if (needs_full_damage) {
      damage_ = output_rect;
    } else {
      // pending_damage_ is in Surface coordinates.
      damage_.Swap(&pending_damage_);
      damage_.Intersect(output_rect);
    }
    pending_damage_.Clear();
  }

  surface_hierarchy_content_bounds_ = gfx::Rect(content_size_);
  if (state_.input_region) {
    hit_test_region_ = *state_.input_region;
    hit_test_region_.Intersect(surface_hierarchy_content_bounds_);
  } else {
    hit_test_region_ = surface_hierarchy_content_bounds_;
  }

  int outset = state_.input_outset;
  if (outset > 0) {
    gfx::Rect input_rect = surface_hierarchy_content_bounds_;
    input_rect.Inset(-outset, -outset);
    hit_test_region_ = input_rect;
  }

  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    gfx::Vector2d offset = sub_surface_entry.second.OffsetFromOrigin();
    // Synchronously commit all pending state of the sub-surface and its
    // descendants.
    sub_surface->CommitSurfaceHierarchy(synchronized);
    surface_hierarchy_content_bounds_.Union(
        sub_surface->surface_hierarchy_content_bounds() + offset);
    hit_test_region_.Union(sub_surface->hit_test_region_ + offset);
  }
}

void Surface::AppendSurfaceHierarchyCallbacks(
    std::list<FrameCallback>* frame_callbacks,
    std::list<PresentationCallback>* presentation_callbacks) {
  // Move frame callbacks to the end of |frame_callbacks|.
  frame_callbacks->splice(frame_callbacks->end(), frame_callbacks_);
  // Move presentation callbacks to the end of |presentation_callbacks|.
  presentation_callbacks->splice(presentation_callbacks->end(),
                                 presentation_callbacks_);

  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    sub_surface->AppendSurfaceHierarchyCallbacks(frame_callbacks,
                                                 presentation_callbacks);
  }
}

void Surface::AppendSurfaceHierarchyContentsToFrame(
    const gfx::Point& origin,
    float device_scale_factor,
    LayerTreeFrameSinkHolder* frame_sink_holder,
    viz::CompositorFrame* frame) {
  // The top most sub-surface is at the front of the RenderPass's quad_list,
  // so we need composite sub-surface in reversed order.
  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    // Synchronsouly commit all pending state of the sub-surface and its
    // decendents.
    sub_surface->AppendSurfaceHierarchyContentsToFrame(
        origin + sub_surface_entry.second.OffsetFromOrigin(),
        device_scale_factor, frame_sink_holder, frame);
  }

  if (needs_update_resource_)
    UpdateResource(frame_sink_holder);

  AppendContentsToFrame(origin, device_scale_factor, frame);

  DCHECK(
      !current_resource_.id ||
      frame_sink_holder->HasReleaseCallbackForResource(current_resource_.id));
}

bool Surface::IsSynchronized() const {
  return delegate_ && delegate_->IsSurfaceSynchronized();
}

bool Surface::IsInputEnabled(Surface* surface) const {
  return !delegate_ || delegate_->IsInputEnabled(surface);
}

bool Surface::HasHitTestRegion() const {
  return !hit_test_region_.IsEmpty();
}

bool Surface::HitTest(const gfx::Point& point) const {
  return hit_test_region_.Contains(point);
}

void Surface::GetHitTestMask(gfx::Path* mask) const {
  hit_test_region_.GetBoundaryPath(mask);
}

void Surface::SetSurfaceDelegate(SurfaceDelegate* delegate) {
  DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;
}

bool Surface::HasSurfaceDelegate() const {
  return !!delegate_;
}

void Surface::AddSurfaceObserver(SurfaceObserver* observer) {
  observers_.AddObserver(observer);
}

void Surface::RemoveSurfaceObserver(SurfaceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Surface::HasSurfaceObserver(const SurfaceObserver* observer) const {
  return observers_.HasObserver(observer);
}

std::unique_ptr<base::trace_event::TracedValue> Surface::AsTracedValue() const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("name", window_->layer()->name());
  return value;
}

bool Surface::IsStylusOnly() {
  return window_->GetProperty(kStylusOnlyKey);
}

void Surface::SetStylusOnly() {
  window_->SetProperty(kStylusOnlyKey, true);
}

void Surface::SurfaceHierarchyResourcesLost() {
  // Update resource and full damage are needed for next frame.
  needs_update_resource_ = true;
  for (const auto& sub_surface : sub_surfaces_)
    sub_surface.first->SurfaceHierarchyResourcesLost();
}

bool Surface::FillsBoundsOpaquely() const {
  return !current_resource_has_alpha_ ||
         state_.blend_mode == SkBlendMode::kSrc ||
         state_.opaque_region.Contains(gfx::Rect(content_size_));
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

Surface::State::State() {}

Surface::State::~State() = default;

bool Surface::State::operator==(const State& other) const {
  return other.opaque_region == opaque_region &&
         other.input_region == input_region &&
         other.buffer_scale == buffer_scale &&
         other.buffer_transform == buffer_transform &&
         other.viewport == viewport && other.crop == crop &&
         other.only_visible_on_secure_output == only_visible_on_secure_output &&
         other.blend_mode == blend_mode && other.alpha == alpha;
}

Surface::BufferAttachment::BufferAttachment() {}

Surface::BufferAttachment::~BufferAttachment() {
  if (buffer_)
    buffer_->OnDetach();
}

Surface::BufferAttachment& Surface::BufferAttachment::operator=(
    BufferAttachment&& other) {
  if (buffer_)
    buffer_->OnDetach();
  buffer_ = other.buffer_;
  size_ = other.size_;
  other.buffer_ = base::WeakPtr<Buffer>();
  other.size_ = gfx::Size();
  return *this;
}

base::WeakPtr<Buffer>& Surface::BufferAttachment::buffer() {
  return buffer_;
}

const base::WeakPtr<Buffer>& Surface::BufferAttachment::buffer() const {
  return buffer_;
}

const gfx::Size& Surface::BufferAttachment::size() const {
  return size_;
}

void Surface::BufferAttachment::Reset(base::WeakPtr<Buffer> buffer) {
  size_ = gfx::Size();
  if (buffer) {
    buffer->OnAttach();
    size_ = buffer->GetSize();
  }
  if (buffer_)
    buffer_->OnDetach();
  buffer_ = buffer;
}

void Surface::UpdateResource(LayerTreeFrameSinkHolder* frame_sink_holder) {
  DCHECK(needs_update_resource_);
  needs_update_resource_ = false;
  if (current_buffer_.buffer()) {
    if (current_buffer_.buffer()->ProduceTransferableResource(
            frame_sink_holder, state_.only_visible_on_secure_output,
            &current_resource_)) {
      current_resource_has_alpha_ =
          FormatHasAlpha(current_buffer_.buffer()->GetFormat());
    } else {
      current_resource_.id = 0;
      // Use the buffer's size, so the AppendContentsToFrame() will append
      // a SolidColorDrawQuad with the buffer's size.
      current_resource_.size = current_buffer_.size();
      current_resource_has_alpha_ = false;
    }
  } else {
    current_resource_.id = 0;
    current_resource_.size = gfx::Size();
    current_resource_has_alpha_ = false;
  }
}

void Surface::UpdateBufferTransform() {
  SkMatrix buffer_matrix;
  switch (state_.buffer_transform) {
    case Transform::NORMAL:
      buffer_matrix.setIdentity();
      break;
    case Transform::ROTATE_90:
      buffer_matrix.setSinCos(-1, 0, 0.5f, 0.5f);
      break;
    case Transform::ROTATE_180:
      buffer_matrix.setSinCos(0, -1, 0.5f, 0.5f);
      break;
    case Transform::ROTATE_270:
      buffer_matrix.setSinCos(1, 0, 0.5f, 0.5f);
      break;
  }
  buffer_matrix.postIDiv(state_.buffer_scale, state_.buffer_scale);
  buffer_transform_ = gfx::Transform(buffer_matrix);
}

void Surface::AppendContentsToFrame(const gfx::Point& origin,
                                    float device_scale_factor,
                                    viz::CompositorFrame* frame) {
  const std::unique_ptr<viz::RenderPass>& render_pass =
      frame->render_pass_list.back();
  gfx::Rect output_rect(origin, content_size_);
  gfx::Rect quad_rect(0, 0, 1, 1);

  // Surface bounds are in DIPs, but |damage_rect| and |output_rect| are in
  // pixels, so we need to scale by the |device_scale_factor|.
  gfx::Rect damage_rect = damage_.bounds();
  if (!damage_rect.IsEmpty()) {
    // Outset damage by 1 DIP to as damage is in surface coordinate space and
    // client might not be aware of |device_scale_factor| and the
    // scaling/filtering it requires.
    damage_rect.Inset(-1, -1);
    damage_rect += origin.OffsetFromOrigin();
    damage_rect.Intersect(output_rect);
    render_pass->damage_rect.Union(
        gfx::ConvertRectToPixel(device_scale_factor, damage_rect));
  }

  // Compute the total transformation from post-transform buffer coordinates to
  // target coordinates.
  SkMatrix viewport_to_target_matrix;
  // Scale and offset the normalized space to fit the content size rectangle.
  viewport_to_target_matrix.setScale(
      content_size_.width() * state_.buffer_scale,
      content_size_.height() * state_.buffer_scale);
  viewport_to_target_matrix.postTranslate(origin.x(), origin.y());
  // Convert from DPs to pixels.
  viewport_to_target_matrix.postScale(device_scale_factor, device_scale_factor);

  gfx::Transform quad_to_target_transform(buffer_transform_);
  quad_to_target_transform.ConcatTransform(
      gfx::Transform(viewport_to_target_matrix));

  bool are_contents_opaque = !current_resource_has_alpha_ ||
                             state_.blend_mode == SkBlendMode::kSrc ||
                             state_.opaque_region.Contains(output_rect);

  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(
      quad_to_target_transform, quad_rect /* quad_layer_rect */,
      quad_rect /* visible_quad_layer_rect */, gfx::Rect() /* clip_rect */,
      false /* is_clipped */, are_contents_opaque, state_.alpha /* opacity */,
      SkBlendMode::kSrcOver /* blend_mode */, 0 /* sorting_context_id */);

  if (current_resource_.id) {
    gfx::RectF uv_crop(gfx::SizeF(1, 1));
    if (!state_.crop.IsEmpty()) {
      // The crop rectangle is a post-transformation rectangle. To get the UV
      // coordinates, we need to convert it to normalized buffer coordinates and
      // pass them through the inverse of the buffer transformation.
      uv_crop = gfx::RectF(state_.crop);
      gfx::Size transformed_buffer_size(
          ToTransformedSize(current_resource_.size, state_.buffer_transform));
      if (!transformed_buffer_size.IsEmpty())
        uv_crop.Scale(1.f / transformed_buffer_size.width(),
                      1.f / transformed_buffer_size.height());

      buffer_transform_.TransformRectReverse(&uv_crop);
    }

    // Texture quad is only needed if buffer is not fully transparent.
    if (state_.alpha) {
      viz::TextureDrawQuad* texture_quad =
          render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};

      texture_quad->SetNew(
          quad_state, quad_rect, quad_rect, !are_contents_opaque,
          current_resource_.id, true /* premultiplied_alpha */,
          uv_crop.origin(), uv_crop.bottom_right(),
          SK_ColorTRANSPARENT /* background_color */, vertex_opacity,
          false /* y_flipped */, false /* nearest_neighbor */,
          state_.only_visible_on_secure_output);
      if (current_resource_.is_overlay_candidate)
        texture_quad->set_resource_size_in_pixels(current_resource_.size);
      frame->resource_list.push_back(current_resource_);
    }
  } else {
    viz::SolidColorDrawQuad* solid_quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    solid_quad->SetNew(quad_state, quad_rect, quad_rect, SK_ColorBLACK,
                       false /* force_anti_aliasing_off */);
  }
}

void Surface::UpdateContentSize() {
  gfx::Size content_size;
  if (!state_.viewport.IsEmpty()) {
    content_size = state_.viewport;
  } else if (!state_.crop.IsEmpty()) {
    DLOG_IF(WARNING, !gfx::IsExpressibleAsInt(state_.crop.width()) ||
                         !gfx::IsExpressibleAsInt(state_.crop.height()))
        << "Crop rectangle size (" << state_.crop.size().ToString()
        << ") most be expressible using integers when viewport is not set";
    content_size = gfx::ToCeiledSize(state_.crop.size());
  } else {
    content_size = gfx::ToCeiledSize(
        gfx::ScaleSize(gfx::SizeF(ToTransformedSize(current_buffer_.size(),
                                                    state_.buffer_transform)),
                       1.0f / state_.buffer_scale));
  }

  // Enable/disable sub-surface based on if it has contents.
  if (has_contents())
    window_->Show();
  else
    window_->Hide();

  if (content_size_ != content_size) {
    content_size_ = content_size;
    window_->SetBounds(gfx::Rect(window_->bounds().origin(), content_size_));
  }
}

}  // namespace exo
