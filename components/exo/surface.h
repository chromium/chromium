// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_H_
#define COMPONENTS_EXO_SURFACE_H_

#include <list>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/base/region.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/surface_delegate.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/transform.h"

class SkPath;

namespace ash {
class OutputProtectionDelegate;
}

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace gfx {
class GpuFence;
}

namespace viz {
class CompositorFrame;
}

namespace exo {
class Buffer;
class FrameSinkResourceManager;
class SurfaceObserver;

namespace subtle {
class PropertyHelper;
}

// Counter-clockwise rotations.
enum class Transform { NORMAL, ROTATE_90, ROTATE_180, ROTATE_270 };

// A property key to store the surface Id set by the client.
extern const ui::ClassProperty<int32_t>* const kClientSurfaceIdKey;

// This class represents a rectangular area that is displayed on the screen.
// It has a location, size and pixel contents.
class Surface final : public ui::PropertyHandler {
 public:
  using PropertyDeallocator = void (*)(int64_t value);

  Surface();
  ~Surface() override;

  // Type-checking downcast routine.
  static Surface* AsSurface(const aura::Window* window);

  aura::Window* window() { return window_.get(); }

  // Set a buffer as the content of this surface. A buffer can only be attached
  // to one surface at a time.
  void Attach(Buffer* buffer);
  void Attach(Buffer* buffer, gfx::Vector2d offset);

  gfx::Vector2d GetBufferOffset();

  // Returns whether the surface has an uncommitted attached buffer.
  bool HasPendingAttachedBuffer() const;

  // Describe the regions where the pending buffer is different from the
  // current surface contents, and where the surface therefore needs to be
  // repainted.
  void Damage(const gfx::Rect& rect);

  // Request notification when it's a good time to produce a new frame. Useful
  // for throttling redrawing operations, and driving animations.
  using FrameCallback = base::Callback<void(base::TimeTicks frame_time)>;
  void RequestFrameCallback(const FrameCallback& callback);

  // Request notification when the next frame is displayed. Useful for
  // throttling redrawing operations, and driving animations.
  using PresentationCallback =
      base::Callback<void(const gfx::PresentationFeedback&)>;
  void RequestPresentationCallback(const PresentationCallback& callback);

  // This sets the region of the surface that contains opaque content.
  void SetOpaqueRegion(const cc::Region& region);

  // This sets the region of the surface that can receive pointer and touch
  // events. The region is clipped to the surface bounds.
  void SetInputRegion(const cc::Region& region);
  const cc::Region& hit_test_region() const { return hit_test_region_; }

  // This resets the region of the surface that can receive pointer and touch
  // events to be wide-open. This will be clipped to the surface bounds.
  void ResetInputRegion();

  // This overrides the input region to the surface bounds with an outset.
  // TODO(domlaskowski): Remove this once client-driven resizing is removed.
  void SetInputOutset(int outset);

  // This sets the scaling factor used to interpret the contents of the buffer
  // attached to the surface. Note that if the scale is larger than 1, then you
  // have to attach a buffer that is larger (by a factor of scale in each
  // dimension) than the desired surface size.
  void SetBufferScale(float scale);

  // This sets the transformation used to interpret the contents of the buffer
  // attached to the surface.
  void SetBufferTransform(Transform transform);

  // Functions that control sub-surface state. All sub-surface state is
  // double-buffered and will be applied when Commit() is called.
  void AddSubSurface(Surface* sub_surface);
  void RemoveSubSurface(Surface* sub_surface);
  void SetSubSurfacePosition(Surface* sub_surface, const gfx::Point& position);
  void PlaceSubSurfaceAbove(Surface* sub_surface, Surface* reference);
  void PlaceSubSurfaceBelow(Surface* sub_surface, Surface* sibling);
  void OnSubSurfaceCommit();

  // This sets the surface viewport for scaling.
  void SetViewport(const gfx::Size& viewport);

  // This sets the surface crop rectangle.
  void SetCrop(const gfx::RectF& crop);

  // This sets the only visible on secure output flag, preventing it from
  // appearing in screenshots or from being viewed on non-secure displays.
  void SetOnlyVisibleOnSecureOutput(bool only_visible_on_secure_output);

  // This sets the blend mode that will be used when drawing the surface.
  void SetBlendMode(SkBlendMode blend_mode);

  // This sets the alpha value that will be applied to the whole surface.
  void SetAlpha(float alpha);

  // Request that surface should have the specified frame type.
  void SetFrame(SurfaceFrameType type);

  // Request that surface should use a specific set of frame colors.
  void SetFrameColors(SkColor active_color, SkColor inactive_color);

  // Request that surface should have a specific startup ID string.
  void SetStartupId(const char* startup_id);

  // Request that surface should have a specific application ID string.
  void SetApplicationId(const char* application_id);

  // Request "parent" for surface.
  void SetParent(Surface* parent, const gfx::Point& position);

  // Request that surface should have a specific ID assigned by client.
  void SetClientSurfaceId(int32_t client_surface_id);
  int32_t GetClientSurfaceId() const;

  // Enable embedding of an arbitrary viz surface in this exo surface.
  // If the callback is valid, a SurfaceDrawQuad will be emitted targeting
  // the returned SurfaceId each frame.
  void SetEmbeddedSurfaceId(
      base::RepeatingCallback<viz::SurfaceId()> surface_id_callback);

  // Request that the attached surface buffer at the next commit is associated
  // with a gpu fence to be signaled when the buffer is ready for use.
  void SetAcquireFence(std::unique_ptr<gfx::GpuFence> gpu_fence);
  // Returns whether the surface has an uncommitted acquire fence.
  bool HasPendingAcquireFence() const;

  // Surface state (damage regions, attached buffers, etc.) is double-buffered.
  // A Commit() call atomically applies all pending state, replacing the
  // current state. Commit() is not guaranteed to be synchronous. See
  // CommitSurfaceHierarchy() below.
  void Commit();

  // This will commit all pending state of the surface and its descendants by
  // recursively calling CommitSurfaceHierarchy() for each sub-surface.
  // If |synchronized| is set to false, then synchronized surfaces should not
  // commit pending state.
  void CommitSurfaceHierarchy(bool synchronized);

  // This will append current callbacks for surface and its descendants to
  // |frame_callbacks| and |presentation_callbacks|.
  void AppendSurfaceHierarchyCallbacks(
      std::list<FrameCallback>* frame_callbacks,
      std::list<PresentationCallback>* presentation_callbacks);

  // This will append contents for surface and its descendants to frame.
  void AppendSurfaceHierarchyContentsToFrame(
      const gfx::Point& origin,
      float device_scale_factor,
      FrameSinkResourceManager* resource_manager,
      viz::CompositorFrame* frame);

  // Returns true if surface is in synchronized mode.
  bool IsSynchronized() const;

  // Returns true if surface should receive input events.
  bool IsInputEnabled(Surface* surface) const;

  // Returns false if the hit test region is empty.
  bool HasHitTestRegion() const;

  // Returns true if |point| is inside the surface.
  bool HitTest(const gfx::Point& point) const;

  // Sets |mask| to the path that delineates the hit test region of the surface.
  void GetHitTestMask(SkPath* mask) const;

  // Set the surface delegate.
  void SetSurfaceDelegate(SurfaceDelegate* delegate);

  // Returns true if surface has been assigned a surface delegate.
  bool HasSurfaceDelegate() const;

  // Surface does not own observers. It is the responsibility of the observer
  // to remove itself when it is done observing.
  void AddSurfaceObserver(SurfaceObserver* observer);
  void RemoveSurfaceObserver(SurfaceObserver* observer);
  bool HasSurfaceObserver(const SurfaceObserver* observer) const;

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // Called when the begin frame source has changed.
  void SetBeginFrameSource(viz::BeginFrameSource* begin_frame_source);

  // Returns the active content size.
  const gfx::Size& content_size() const { return content_size_; }

  // Returns the active content bounds for surface hierarchy. ie. the bounding
  // box of the surface and its descendants, in the local coordinate space of
  // the surface.
  const gfx::Rect& surface_hierarchy_content_bounds() const {
    return surface_hierarchy_content_bounds_;
  }

  // Returns true if the associated window is in 'stylus-only' mode.
  bool IsStylusOnly();

  // Enables 'stylus-only' mode for the associated window.
  void SetStylusOnly();

  // Notify surface that resources and subsurfaces' resources have been lost.
  void SurfaceHierarchyResourcesLost();

  // Returns true if the surface's bounds should be filled opaquely.
  bool FillsBoundsOpaquely() const;

  bool HasPendingDamageForTesting(const gfx::Rect& damage) const {
    return pending_damage_.Contains(damage);
  }

  // Set occlusion tracking region for surface.
  void SetOcclusionTracking(bool tracking);

  // Triggers sending an occlusion update to observers.
  void OnWindowOcclusionChanged();

  // True if the window for this surface has its occlusion tracked.
  bool is_tracking_occlusion() const { return is_tracking_occlusion_; }

  // Sets the |surface_hierarchy_content_bounds_|.
  void SetSurfaceHierarchyContentBoundsForTest(const gfx::Rect& content_bounds);

 private:
  struct State {
    State();
    ~State();

    bool operator==(const State& other) const;
    bool operator!=(const State& other) const { return !(*this == other); }

    cc::Region opaque_region;
    base::Optional<cc::Region> input_region;
    int input_outset = 0;
    float buffer_scale = 1.0f;
    Transform buffer_transform = Transform::NORMAL;
    gfx::Size viewport;
    gfx::RectF crop;
    bool only_visible_on_secure_output = false;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;
    float alpha = 1.0f;
    gfx::Vector2d offset;
  };
  class BufferAttachment {
   public:
    BufferAttachment();
    ~BufferAttachment();

    BufferAttachment& operator=(BufferAttachment&& buffer);

    base::WeakPtr<Buffer>& buffer();
    const base::WeakPtr<Buffer>& buffer() const;
    const gfx::Size& size() const;
    void Reset(base::WeakPtr<Buffer> buffer);

   private:
    base::WeakPtr<Buffer> buffer_;
    gfx::Size size_;

    DISALLOW_COPY_AND_ASSIGN(BufferAttachment);
  };

  friend class subtle::PropertyHelper;

  // Updates current_resource_ with a new resource id corresponding to the
  // contents of the attached buffer (or id 0, if no buffer is attached).
  // UpdateSurface must be called afterwards to ensure the release callback
  // will be called.
  void UpdateResource(FrameSinkResourceManager* resource_manager);

  // Updates buffer_transform_ to match the current buffer parameters.
  void UpdateBufferTransform(bool y_invert);

  // Puts the current surface into a draw quad, and appends the draw quads into
  // the |frame|.
  void AppendContentsToFrame(const gfx::Point& origin,
                             float device_scale_factor,
                             viz::CompositorFrame* frame);

  // Update surface content size base on current buffer size.
  void UpdateContentSize();

  // This returns true when the surface has some contents assigned to it.
  bool has_contents() const { return !current_buffer_.size().IsEmpty(); }

  // This window has the layer which contains the Surface contents.
  std::unique_ptr<aura::Window> window_;

  // This true, if sub_surfaces_ has changes (order, position, etc).
  bool sub_surfaces_changed_ = false;

  // This is the size of the last committed contents.
  gfx::Size content_size_;

  // This is the bounds of the last committed surface hierarchy contents.
  gfx::Rect surface_hierarchy_content_bounds_;

  // This is true when Attach() has been called and new contents should take
  // effect next time Commit() is called.
  bool has_pending_contents_ = false;

  // The buffer that will become the content of surface when Commit() is called.
  BufferAttachment pending_buffer_;

  // The damage region to schedule paint for when Commit() is called.
  cc::Region pending_damage_;

  // The damage region which will be used by
  // AppendSurfaceHierarchyContentsToFrame() to generate frame.
  cc::Region damage_;

  // These lists contains the callbacks to notify the client when it is a good
  // time to start producing a new frame. These callbacks move to
  // |frame_callbacks_| when Commit() is called. Later they are moved to
  // |active_frame_callbacks_| when the effect of the Commit() is scheduled to
  // be drawn. They fire at the first begin frame notification after this.
  std::list<FrameCallback> pending_frame_callbacks_;
  std::list<FrameCallback> frame_callbacks_;

  // These lists contains the callbacks to notify the client when surface
  // contents have been presented. These callbacks move to
  // |presentation_callbacks_| when Commit() is called. Later they are moved to
  // |swapping_presentation_callbacks_| when the effect of the Commit() is
  // scheduled to be drawn and then moved to |swapped_presentation_callbacks_|
  // after receiving VSync parameters update for the previous frame. They fire
  // at the next VSync parameters update after that.
  std::list<PresentationCallback> pending_presentation_callbacks_;
  std::list<PresentationCallback> presentation_callbacks_;

  // This is the state that has yet to be committed.
  State pending_state_;

  // This is the state that has been committed.
  State state_;

  // Cumulative input region of surface and its sub-surfaces.
  cc::Region hit_test_region_;

  // The stack of sub-surfaces to take effect when Commit() is called.
  // Bottom-most sub-surface at the front of the list and top-most sub-surface
  // at the back.
  using SubSurfaceEntry = std::pair<Surface*, gfx::Point>;
  using SubSurfaceEntryList = std::list<SubSurfaceEntry>;
  SubSurfaceEntryList pending_sub_surfaces_;
  SubSurfaceEntryList sub_surfaces_;

  // The buffer that is currently set as content of surface.
  BufferAttachment current_buffer_;

  // The last resource that was sent to a surface.
  viz::TransferableResource current_resource_;

  // Whether the last resource that was sent to a surface has an alpha channel.
  bool current_resource_has_alpha_ = false;

  // The acquire gpu fence to associate with the surface buffer when Commit()
  // is called.
  std::unique_ptr<gfx::GpuFence> pending_acquire_fence_;
  // The acquire gpu fence that is currently associated with the surface buffer.
  std::unique_ptr<gfx::GpuFence> acquire_fence_;

  // This is true if a call to Commit() as been made but
  // CommitSurfaceHierarchy() has not yet been called.
  bool needs_commit_surface_ = false;

  // This is true if UpdateResources() should be called.
  bool needs_update_resource_ = true;

  // The current buffer transform matrix. It specifies the transformation from
  // normalized buffer coordinates to post-tranform buffer coordinates.
  gfx::Transform buffer_transform_;

  // This is set when the compositing starts and passed to active frame
  // callbacks when compositing successfully ends.
  base::TimeTicks last_compositing_start_time_;

  // This can be set to have some functions delegated. E.g. ShellSurface class
  // can set this to handle Commit() and apply any double buffered state it
  // maintains.
  SurfaceDelegate* delegate_ = nullptr;

  // Surface observer list. Surface does not own the observers.
  base::ObserverList<SurfaceObserver, true>::Unchecked observers_;

  // Whether this surface is tracking occlusion for the client.
  bool is_tracking_occlusion_ = false;

#if defined(OS_CHROMEOS)
  std::unique_ptr<ash::OutputProtectionDelegate> output_protection_;
#endif  // defined(OS_CHROMEOS)

  viz::SurfaceId first_embedded_surface_id_;
  viz::SurfaceId latest_embedded_surface_id_;
  base::RepeatingCallback<viz::SurfaceId()> get_current_surface_id_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

class ScopedSurface {
 public:
  ScopedSurface(Surface* surface, SurfaceObserver* observer);
  ~ScopedSurface();
  Surface* get() { return surface_; }

 private:
  Surface* const surface_;
  SurfaceObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_H_
