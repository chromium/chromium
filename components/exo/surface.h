// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_H_
#define COMPONENTS_EXO_SURFACE_H_

#include <list>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "cc/base/region.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "components/exo/buffer.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/surface_delegate.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/native_widget_types.h"

class SkPath;

namespace ash {
class OutputProtectionDelegate;
}

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace gfx {
class ColorSpace;
class GpuFence;
struct PresentationFeedback;
}  // namespace gfx

namespace viz {
class CompositorFrame;
}

namespace exo {

// Occluded surfaces can be detected and not emitted as a quad in the
// corresponding compositor frame.
BASE_DECLARE_FEATURE(kExoPerSurfaceOcclusion);

class Buffer;
class SecurityDelegate;
class FrameSinkResourceManager;
class SurfaceObserver;

namespace subtle {
class PropertyHelper;
}

// Counter-clockwise rotations.
enum class Transform {
  NORMAL,
  ROTATE_90,
  ROTATE_180,
  ROTATE_270,
  FLIPPED,
  FLIPPED_ROTATE_90,
  FLIPPED_ROTATE_180,
  FLIPPED_ROTATE_270
};

// Priority for overlay promotion.
enum class OverlayPriority { LOW, REGULAR, REQUIRED };

// A property key to store the surface Id set by the client.
extern const ui::ClassProperty<std::string*>* const kClientSurfaceIdKey;

// A property key to store the window session Id set by client or full_restore
// component.
extern const ui::ClassProperty<int32_t>* const kWindowSessionId;

// This class represents a rectangular area that is displayed on the screen.
// It has a location, size and pixel contents.
class Surface final : public ui::PropertyHandler {
 public:
  using PropertyDeallocator = void (*)(int64_t value);
  using LeaveEnterCallback = base::RepeatingCallback<bool(int64_t, int64_t)>;

  Surface();

  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;

  ~Surface() override;

  // Type-checking downcast routine.
  static Surface* AsSurface(const aura::Window* window);

  aura::Window* window() const { return window_.get(); }

  std::vector<raw_ptr<aura::Window, VectorExperimental>> GetChildWindows()
      const;

  void set_leave_enter_callback(LeaveEnterCallback callback) {
    leave_enter_callback_ = callback;
  }

  void set_legacy_buffer_release_skippable(bool skippable) {
    legacy_buffer_release_skippable_ = skippable;
  }

  bool is_augmented() const { return is_augmented_; }
  void set_is_augmented(bool augmented) { is_augmented_ = augmented; }

  // Called when the display the surface is on has changed.
  // Returns true if successful, and false if it fails.
  bool UpdateDisplay(int64_t old_id, int64_t new_id);

  display::Display GetDisplay() const;

  // Called when the output is added for new display.
  void OnNewOutputAdded();

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
  using FrameCallback =
      base::RepeatingCallback<void(base::TimeTicks frame_time)>;
  void RequestFrameCallback(const FrameCallback& callback);

  // Request notification when the next frame is displayed. Useful for
  // throttling redrawing operations, and driving animations.
  using PresentationCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback&)>;
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
  // Allow for finer granularity for sub surface positioning.
  void SetSubSurfacePosition(Surface* sub_surface, const gfx::PointF& position);
  void PlaceSubSurfaceAbove(Surface* sub_surface, Surface* reference);
  void PlaceSubSurfaceBelow(Surface* sub_surface, Surface* sibling);
  void OnSubSurfaceCommit();

  using SubSurfaceEntry = std::pair<Surface*, gfx::PointF>;
  using SubSurfaceEntryList = std::list<SubSurfaceEntry>;
  SubSurfaceEntryList& sub_surfaces() { return sub_surfaces_; }
  SubSurfaceEntryList& render_layers() { return render_layers_; }

  // `rounded_corners_bounds` is on the local surface coordinates.
  // If `commit` is true, rounded corner bounds are add to committed state,
  // overriding the previously committed value.
  void SetRoundedCorners(const gfx::RRectF& rounded_corners_bounds,
                         bool commit_override);
  void SetOverlayPriorityHint(OverlayPriority hint);

  // Sets the surface's clip rectangle.
  void SetClipRect(const std::optional<gfx::RectF>& clip_rect);

  // Sets the trace ID for tracking frame submission, which is used for the next
  // surface commit.
  void SetFrameTraceId(int64_t frame_trace_id);

  // Sets the surface's transformation matrix.
  void SetSurfaceTransform(const gfx::Transform& transform);

  // Sets the background color that shall be associated with the next buffer
  // commit.
  void SetBackgroundColor(std::optional<SkColor4f> background_color);

  // This sets the surface viewport for scaling.
  void SetViewport(const gfx::SizeF& viewport);

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

  // Request that the server should start resize on this surface.
  void SetServerStartResize();

  // Request that surface should use a specific set of frame colors.
  void SetFrameColors(SkColor active_color, SkColor inactive_color);

  // Request that surface should have a specific startup ID string.
  void SetStartupId(const char* startup_id);

  // Request that surface should have a specific application ID string.
  void SetApplicationId(const char* application_id);

  // Whether to show/hide the shelf when fullscreen. If true, the titlebar/shelf
  // will show when the mouse moves to the top/bottom of the screen. If false
  // (plain fullscreen), the titlebar and shelf are always hidden.
  void SetUseImmersiveForFullscreen(bool value);

  // Called to show the snap preview to the primary or secondary position, or
  // to hide it.
  void ShowSnapPreviewToSecondary();
  void ShowSnapPreviewToPrimary();
  void HideSnapPreview();

  // Called when the client was snapped to primary or secondary position, or
  // reset.
  void SetSnapPrimary(float snap_ratio);
  void SetSnapSecondary(float snap_ratio);
  void UnsetSnap();

  // Whether the current client window can go back, as per its navigation list.
  void SetCanGoBack();
  void UnsetCanGoBack();

  // This sets the color space for the buffer for this surface.
  void SetColorSpace(gfx::ColorSpace color_space);

  // Request "parent" for surface.
  void SetParent(Surface* parent, const gfx::Point& position);

  // Request that surface should have a specific ID assigned by client.
  void SetClientSurfaceId(const char* client_surface_id);
  std::string GetClientSurfaceId() const;

  // Sets whether the surface contains video.
  void SetContainsVideo(bool contains_video);

  // Returns whether this surface or any of its subsurfaces contains a video.
  bool ContainsVideo();

  // Request that the attached surface buffer at the next commit is associated
  // with a gpu fence to be signaled when the buffer is ready for use.
  void SetAcquireFence(std::unique_ptr<gfx::GpuFence> gpu_fence);
  // Returns whether the surface has an uncommitted acquire fence.
  bool HasPendingAcquireFence() const;
  // Returns whether the surface has a committed acquire fence.
  bool HasAcquireFence() const;

  // Request a callback when the buffer attached at the next commit is
  // no longer used by that commit.
  void SetPerCommitBufferReleaseCallback(
      Buffer::PerCommitExplicitReleaseCallback callback);
  // Whether the surface has an uncommitted per-commit buffer release callback.
  bool HasPendingPerCommitBufferReleaseCallback() const;

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
      const gfx::PointF& parent_to_root_px,
      const gfx::PointF& to_parent_dp,
      bool needs_full_damage,
      FrameSinkResourceManager* resource_manager,
      std::optional<float> device_scale_factor,
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

  // Returns a pointer to the SurfaceDelegate for this surface, used by tests.
  SurfaceDelegate* GetDelegateForTesting();

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
  const gfx::SizeF& content_size() const { return content_size_; }

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
    return pending_state_.damage.Contains(damage);
  }

  bool HasLeaveEnterCallbackForTesting() const {
    return !leave_enter_callback_.is_null();
  }

  // Set occlusion tracking region for surface.
  void SetOcclusionTracking(bool tracking);

  void OnScaleFactorChanged(float old_scale_factor, float new_scale_factor);

  // Triggers sending an occlusion update to observers.
  void OnWindowOcclusionChanged(
      aura::Window::OcclusionState old_occlusion_state,
      aura::Window::OcclusionState new_occlusion_state);

  // Triggers sending a locking status to observers.
  // true : lock a frame to normal or restore state
  // false : unlock the previously locked frame
  void SetFrameLocked(bool lock);

  // True if the window for this surface has its occlusion tracked.
  bool IsTrackingOcclusion();

  // Sets the |surface_hierarchy_content_bounds_|.
  void SetSurfaceHierarchyContentBoundsForTest(const gfx::Rect& content_bounds);

  // Requests that this surface should be made active (i.e. foregrounded).
  void RequestActivation();

  // Requests that surface my have a window session ID assigned by client or
  // full_restore component.
  void SetWindowSessionId(int32_t window_session_id);
  int32_t GetWindowSessionId();

  // Requests that the surface enters PIP mode.
  void SetPip();

  // Requests that the surface exits PIP mode.
  void UnsetPip();

  // Requests that the surface maintains the given aspect ratio.
  void SetAspectRatio(const gfx::SizeF& aspect_ratio);

  // Triggers send desk state of the window to observers.
  // |state| is the index of the desk which the window moved to,
  // or -1 for a window assigned to all desks.
  void OnDeskChanged(int state);

  // Requests that DesksController to move the window to a desk at |desk_index|.
  void MoveToDesk(int desk_index);

  // Requests that window is visible on all workspaces.
  void SetVisibleOnAllWorkspaces();

  // Sets the initial workspace to restore a window to the corresponding desk.
  void SetInitialWorkspace(const char* initial_workspace);

  // Pins/locks a window to the screen so that the user cannot do anything
  // else before the mode is released. If trusted is set, it is an invocation
  // from a trusted app like a school test mode app.
  void Pin(bool trusted);

  // Release the pinned mode and allows the user to do other things again.
  void Unpin();

  // Starts or ends throttling on the surface.
  void ThrottleFrameRate(bool on);

  // Informs tooltip is shown.
  void OnTooltipShown(const std::u16string& text, const gfx::Rect& bounds);

  // Informs tooltip is hidden.
  void OnTooltipHidden();

  // If true is set, if this window has a focus, key events should be sent to
  // the app, even if it is an ash shortcut (with some exceptions).
  // See exo::Keyboard for more details.
  void SetKeyboardShortcutsInhibited(bool inhibited);

  // Returns whether keyboard shortcuts are inhibited.
  bool is_keyboard_shortcuts_inhibited() const {
    return keyboard_shortcuts_inhibited_;
  }

  // Returns the SecurityDelegate associated with this surface, or nullptr
  // if one can not be determined. See go/secure-exo-ids for more details.
  SecurityDelegate* GetSecurityDelegate();

  // Sets the accessibility window ID sent from the shell client to the window.
  // A negative number removes it.
  void SetClientAccessibilityId(int id);

  // Set top inset for surface.
  void SetTopInset(int height);

  // Inform observers and subsurfaces about new fullscreen state
  void OnFullscreenStateChanged(bool fullscreen);

  OverlayPriority GetOverlayPriorityHint() {
    return state_.overlay_priority_hint;
  }

  // Returns the buffer scale of the last committed buffer.
  float GetBufferScale() const { return state_.basic_state.buffer_scale; }

  int64_t GetFrameTraceId() const { return state_.frame_trace_id; }

  // Returns the last committed buffer.
  Buffer* GetBuffer();

  // Dump Debug Info.
  std::string DumpDebugInfo() const;

 private:
  struct State {
    State();
    ~State();

    bool operator==(const State& other) const;
    bool operator!=(const State& other) const { return !(*this == other); }

    cc::Region opaque_region;
    std::optional<cc::Region> input_region;
    int input_outset = 0;
    float buffer_scale = 1.0f;
    Transform buffer_transform = Transform::NORMAL;
    gfx::SizeF viewport;
    gfx::RectF crop;
    bool only_visible_on_secure_output = false;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;
    float alpha = 1.0f;
    gfx::Vector2d offset;
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    bool is_tracking_occlusion = false;
    // Represents optional background color that must be associated with the
    // next buffer commit.
    std::optional<SkColor4f> background_color;
    bool contains_video = false;
  };
  class BufferAttachment {
   public:
    BufferAttachment();

    BufferAttachment(const BufferAttachment&) = delete;
    BufferAttachment& operator=(const BufferAttachment&) = delete;

    ~BufferAttachment();

    BufferAttachment(BufferAttachment&& buffer);
    BufferAttachment& operator=(BufferAttachment&& buffer);

    base::WeakPtr<Buffer>& buffer();
    const base::WeakPtr<Buffer>& buffer() const;
    const gfx::Size& size() const;
    void Reset(base::WeakPtr<Buffer> buffer);

   private:
    base::WeakPtr<Buffer> buffer_;
    gfx::Size size_;
  };

  // State for this surface. State is committed in a three step process:
  // 1. Pending state is accummulated into before commit.
  // 2. On commit, state is copied to a cached state. This is to support
  //    synchronized commit of a tree of surfaces. When the tree of surfaces is
  //    set to be synchronized, the state of the tree will not be committed
  //    until the root of the tree (precisely, until a unsynchronized root of a
  //    subtree) is committed.
  // 3. State is committed.
  // Some fields are persisted between commits (e.g. which buffer is attached),
  // and some fields are not (e.g. acquire fence). For fields that are
  // persisted, they either need to be copyable, or if they are move only, they
  // need to be wrapped in std::optional and only copied on commit if they
  // have been changed. Not doing this can lead to broken behaviour, such as
  // losing the attached buffer if some unrelated field is updated in a commit.
  // If you add new fields to this struct, please document whether the field
  // should be persisted between commits.
  // See crbug.com/1283305 for context.
  struct ExtendedState {
    ExtendedState();
    ~ExtendedState();

    State basic_state;

    // The buffer that will become the content of surface.
    // Persisted between commits.
    std::optional<BufferAttachment> buffer;
    // The rounded corners bounds for the surface.
    // Persisted between commits.
    gfx::RRectF rounded_corners_bounds;
    // The damage region to schedule paint for.
    // Not persisted between commits.
    cc::Region damage;
    // These lists contain the callbacks to notify the client when it is a good
    // time to start producing a new frame.
    // Not persisted between commits.
    std::list<FrameCallback> frame_callbacks;
    // These lists contain the callbacks to notify the client when surface
    // contents have been presented.
    // Not persisted between commits.
    std::list<PresentationCallback> presentation_callbacks;
    // The acquire gpu fence to associate with the surface buffer.
    // Not persisted between commits.
    std::unique_ptr<gfx::GpuFence> acquire_fence;
    // Callback to notify about the per-commit buffer release. The wayland
    // Exo backend uses this callback to implement the immediate_release
    // event of the explicit sync protocol.
    // Not persisted between commits.
    Buffer::PerCommitExplicitReleaseCallback
        per_commit_explicit_release_callback_;
    // The hint for overlay prioritization
    // Persisted between commits.
    OverlayPriority overlay_priority_hint = OverlayPriority::REGULAR;
    // The clip rect for this surface, in the local coordinate space. This
    // should only be set for subsurfaces.
    // Persisted between commits.
    std::optional<gfx::RectF> clip_rect;
    // The transform to apply when drawing this surface. This should only be set
    // for subsurfaces, and doesn't apply to children of this surface.
    // Persisted between commits.
    gfx::Transform surface_transform;

    // Trace ID for tracking frame submission.
    // Not persisted between commits.
    int64_t frame_trace_id = -1;
  };

  friend class subtle::PropertyHelper;

  // Adjust the stacking order of `list`, returns true if the `list` ordering is
  // altered.
  bool DoPlaceAboveOrBelow(Surface* child,
                           Surface* reference,
                           SubSurfaceEntryList& list,
                           bool place_above);

  // Updates current_resource_ with a new resource id corresponding to the
  // contents of the attached buffer (or id 0, if no buffer is attached).
  // UpdateSurface must be called afterwards to ensure the release callback
  // will be called.
  void UpdateResource(FrameSinkResourceManager* resource_manager);

  // Updates buffer_transform_ to match the current buffer parameters.
  void UpdateBufferTransform(bool y_invert);

  // Update state_.overlay_priority_hint and notify observers
  void UpdateOverlayPriorityHint(OverlayPriority overlay_priority_hint);

  // Puts the current surface into a draw quad, and appends the draw quads into
  // the `frame`. `device_scale_factor` is supplied if the client does not
  // submit surfaces in pixel coordinates.
  void AppendContentsToFrame(const gfx::PointF& parent_to_root_px,
                             const gfx::PointF& to_parent_dp,
                             bool needs_full_damage,
                             std::optional<float> device_scale_factor,
                             viz::CompositorFrame* frame);

  // Update surface content size base on current buffer size.
  void UpdateContentSize();

  // This returns true when the surface has some contents assigned to it.
  bool has_contents() const {
    return state_.buffer.has_value() && !state_.buffer->size().IsEmpty();
  }

  // This window has the layer which contains the Surface contents.
  std::unique_ptr<aura::Window> window_;

  // Whether this surface is an object only to composite its parent.
  bool is_augmented_ = false;

  // This is true, if sub_surfaces_ has changes (order, position, etc).
  bool sub_surfaces_changed_ = false;

  // Because client side damage does not expand past `content_size_`. This
  // accounts for damage that are outside of this surface. 3 ways extended
  // damage can be introduced if this surface is a subsurface that fits within
  // the overall shell_surface's host_window bounds:
  //
  // 1) This surface's width/height shrinks without changing stacking/position,
  // it will not fully damage the parent surface, introduced damage area (dotted
  // line). s1 (child of s) shrunk:
  //
  //  _host_window__________         _host_window__________
  // |s    ______           |       |s    ____...          |
  // |    |s1    |          |       |    |s1  | :          |
  // |    |      |          |  =>   |    |    | :          |
  // |    |      |          |       |    |____| :          |
  // |    |______|          |       |    :......:          |
  // |______________________|       |______________________|
  //
  // 2) The toplevel surface shrinks but not in a way that affects host_window
  // bounds due to a subsurface expanding it, introduced damage area (dotted
  // line). s (parent of s1) shrunk:
  //
  //  _host_window__________         _host_window__________
  // |s    ______           |       |s    ______           |
  // |    |s1    |          |       |    |s1    |          |
  // |    |      |          |  =>   |____|      |__________|
  // |    |      |          |       :    |      |          :
  // |____|______|__________|       :....|______|..........:
  //
  // 3) This surface has a subsurface that is shown outside, but fits within the
  // overall shell_surface's host_window, when the subsurface is removed, it
  // fully damage the parent surface, but not the part outside of the parent
  // (dotted line). s11 (child of s1) is removed:
  //
  //  _host_window__________         _host_window__________
  // |s0   __________       |       |s0   __________       |
  // |    |s1        |      |       |    |s1        |      |
  // |    |  ______  |      |  =>   |    |          |      |
  // |    |_|s11   |_|      |       |    |__________|      |
  // |      |______|        |       |      :......:        |
  // |______________________|       |______________________|
  //
  std::optional<gfx::RectF> extended_damage_dp_ = std::nullopt;

  // This is the size of the last committed contents.
  gfx::SizeF content_size_;

  // This is the bounds of the last committed surface hierarchy contents.
  gfx::Rect surface_hierarchy_content_bounds_;

  // This is true when Attach() has been called and new contents should be
  // cached next time Commit() is called.
  bool has_pending_contents_ = false;
  // This is true when new contents are cached and should take effect next time
  // synchronized CommitSurfaceHierarchy() is called.
  bool has_cached_contents_ = false;

  // This is the state that has yet to be cached.
  ExtendedState pending_state_;
  // This is the state that has yet to be committed.
  ExtendedState cached_state_;
  // This is the state that has been committed.
  ExtendedState state_;

  // Cumulative input region of surface and its sub-surfaces.
  cc::Region hit_test_region_;

  // The stack of sub-surfaces to take effect when Commit() is called.
  // Bottom-most sub-surface at the front of the list and top-most sub-surface
  // at the back.
  SubSurfaceEntryList pending_sub_surfaces_;
  SubSurfaceEntryList sub_surfaces_;

  // The stack of delegate compositing render_layers for this surface when
  // Commit() is called.
  // The tree structure of this with sub_surface is this (Surface2 is stacked
  // beneath Surface3):
  //
  //             Surface1: { layer1, layer2 }
  //            /         \
  //           /           \
  // Surface2: { layer3 }   \
  //                       Surface3: { layer4, layer5 }
  //
  // When compositing, from bottom to top, the content order is visually:
  // { Surface1, layer1, layer2, Surface2, layer3, Surface3, layer4, layer5 }
  //
  // TODO(fangzhoug): Reusing wl_subsurface and SubSurface class is not ideal,
  // consider introducing a different role object like wl_subsurface, or a base
  // object like wl_surface, to better prevent the unintended behavior such has
  // a layer parenting a subsurface.
  SubSurfaceEntryList render_layers_;

  // The last resource that was sent to a surface.
  viz::TransferableResource current_resource_;

  // Whether the last resource that was sent to a surface has an alpha channel.
  bool current_resource_has_alpha_ = false;

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
  raw_ptr<SurfaceDelegate> delegate_ = nullptr;

  // Surface observer list. Surface does not own the observers.
  base::ObserverList<SurfaceObserver, true>::Unchecked observers_;

  std::unique_ptr<ash::OutputProtectionDelegate> output_protection_;

  LeaveEnterCallback leave_enter_callback_;

  bool keyboard_shortcuts_inhibited_ = false;
  bool legacy_buffer_release_skippable_ = false;

  // Display id state for unmapped surfaces.
  int64_t display_id_ = display::kInvalidDisplayId;
};

class ScopedSurface {
 public:
  ScopedSurface(Surface* surface, SurfaceObserver* observer);

  ScopedSurface(const ScopedSurface&) = delete;
  ScopedSurface& operator=(const ScopedSurface&) = delete;

  virtual ~ScopedSurface();
  Surface* get() { return surface_; }

 private:
  const raw_ptr<Surface> surface_;
  const raw_ptr<SurfaceObserver> observer_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_H_
