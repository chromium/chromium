// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_base.h"

#include <stdint.h>

#include "ash/display/screen_orientation_controller.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/public/cpp/rounded_corner_utils.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/traced_value.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/custom_window_state_delegate.h"
#include "components/exo/security_delegate.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/window_properties.h"
#include "components/exo/wm_helper.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/class_property.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

// The accelerator keys used to close ShellSurfaces.
const struct {
  ui::KeyboardCode keycode;
  int modifiers;
} kCloseWindowAccelerators[] = {
    {ui::VKEY_W, ui::EF_CONTROL_DOWN},
    {ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
    {ui::VKEY_F4, ui::EF_ALT_DOWN}};

class ShellSurfaceWidget : public views::Widget {
 public:
  ShellSurfaceWidget() = default;

  ShellSurfaceWidget(const ShellSurfaceWidget&) = delete;
  ShellSurfaceWidget& operator=(const ShellSurfaceWidget&) = delete;

  // Overridden from views::Widget:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (GetFocusManager()->GetFocusedView() &&
        GetFocusManager()->GetFocusedView()->GetWidget() != this) {
      // If the focus is on the overlay widget, dispatch the key event normally.
      views::Widget::OnKeyEvent(event);
    } else if (GetFocusManager()->ProcessAccelerator(ui::Accelerator(*event))) {
      // Otherwise handle only accelerators. Do not call Widget::OnKeyEvent that
      // eats focus management keys (like the tab key) as well.
      event->SetHandled();
    }
  }
};

class CustomFrameView : public ash::NonClientFrameViewAsh {
 public:
  using ShapeRects = std::vector<gfx::Rect>;

  CustomFrameView(views::Widget* widget,
                  ShellSurfaceBase* shell_surface,
                  bool enabled)
      : NonClientFrameViewAsh(widget), shell_surface_(shell_surface) {
    SetFrameEnabled(enabled);
    if (!enabled)
      NonClientFrameViewAsh::SetShouldPaintHeader(false);
  }

  CustomFrameView(const CustomFrameView&) = delete;
  CustomFrameView& operator=(const CustomFrameView&) = delete;

  ~CustomFrameView() override = default;

  // Overridden from ash::NonClientFrameViewAsh:
  void SetShouldPaintHeader(bool paint) override {
    if (GetFrameEnabled()) {
      NonClientFrameViewAsh::SetShouldPaintHeader(paint);
      return;
    }
  }

  // Overridden from views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override {
    if (GetFrameEnabled())
      return ash::NonClientFrameViewAsh::GetBoundsForClientView();
    return bounds();
  }

  // Overridden from views::NonClientFrameView:
  void UpdateWindowRoundedCorners() override {
    if (!chromeos::features::IsRoundedWindowsEnabled() && GetFrameEnabled()) {
      header_view_->SetHeaderCornerRadius(
          chromeos::GetFrameCornerRadius(frame()->GetNativeWindow()));
    }

    if (!GetWidget()) {
      return;
    }

    aura::Window* window = GetWidget()->GetNativeWindow();
    const ash::WindowState* window_state = ash::WindowState::Get(window);
    std::optional<gfx::RoundedCornersF> window_radii =
        shell_surface_->window_corners_radii();
    std::optional<gfx::RoundedCornersF> shadow_radii =
        shell_surface_->shadow_corner_radii();

    int corner_radius = -1;
    if (window_state->IsPip()) {
      corner_radius = chromeos::kPipRoundedCornerRadius;
    } else if (window_radii || shadow_radii) {
      gfx::RoundedCornersF radii;

      // Certain clients (such as Lacros, for instance) handle window rounding
      // on the client side. These clients do not specify window_radii. However,
      // they do specify shadow radii which matches window radii.
      // For such clients, use shadow radii to specify
      // `aura::client::kWindowCornerRadiusKey` since it is used to round
      // various server side decorations.
      radii =
          window_radii.value_or(shadow_radii.value_or(gfx::RoundedCornersF()));

      // TODO(crbug.com/40256581): Support variable window radii.
      corner_radius = radii.upper_left();
    }

    // Various window decorations are rounded using `kWindowCornerRadiusKey`
    // property.
    window->SetProperty(aura::client::kWindowCornerRadiusKey, corner_radius);

    // If window_radii is null, skip rounding the window.
    if (!window_radii) {
      return;
    }

    if (GetFrameEnabled()) {
      header_view_->SetHeaderCornerRadius(corner_radius);
    }

    GetWidget()->client_view()->UpdateWindowRoundedCorners(corner_radius);
  }

  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    if (GetFrameEnabled()) {
      return ash::NonClientFrameViewAsh::GetWindowBoundsForClientBounds(
          client_bounds);
    }
    return client_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override {
    if (GetFrameEnabled() || shell_surface_->server_side_resize()) {
      return ash::NonClientFrameViewAsh::NonClientHitTest(point);
    }
    return GetWidget()->client_view()->NonClientHitTest(point);
  }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {
    if (GetFrameEnabled())
      return ash::NonClientFrameViewAsh::GetWindowMask(size, window_mask);
  }
  void ResetWindowControls() override {
    if (GetFrameEnabled())
      return ash::NonClientFrameViewAsh::ResetWindowControls();
  }
  void UpdateWindowIcon() override {
    if (GetFrameEnabled())
      return ash::NonClientFrameViewAsh::ResetWindowControls();
  }
  void UpdateWindowTitle() override {
    if (GetFrameEnabled())
      return ash::NonClientFrameViewAsh::UpdateWindowTitle();
  }
  void SizeConstraintsChanged() override {
    if (GetFrameEnabled())
      return ash::NonClientFrameViewAsh::SizeConstraintsChanged();
  }
  gfx::Size GetMinimumSize() const override {
    gfx::Size minimum_size = shell_surface_->GetMinimumSize();
    if (GetFrameEnabled()) {
      return ash::NonClientFrameViewAsh::GetWindowBoundsForClientBounds(
                 gfx::Rect(minimum_size))
          .size();
    }
    return minimum_size;
  }
  gfx::Size GetMaximumSize() const override {
    gfx::Size maximum_size = shell_surface_->GetMaximumSize();
    if (GetFrameEnabled() && !maximum_size.IsEmpty()) {
      return ash::NonClientFrameViewAsh::GetWindowBoundsForClientBounds(
                 gfx::Rect(maximum_size))
          .size();
    }
    return maximum_size;
  }

 private:
  const raw_ptr<ShellSurfaceBase> shell_surface_;
};

class CustomClientView : public views::ClientView {
 public:
  CustomClientView(views::Widget* widget, ShellSurfaceBase* shell_surface)
      : views::ClientView(widget, shell_surface),
        shell_surface_(shell_surface) {}

  CustomClientView(const CustomClientView&) = delete;
  CustomClientView& operator=(const CustomClientView&) = delete;

  ~CustomClientView() override = default;

  // ClientView:
  void UpdateWindowRoundedCorners(int corner_radius) override {
    DCHECK(GetWidget());
    const CustomFrameView* custom_frame_view = static_cast<CustomFrameView*>(
        GetWidget()->non_client_view()->frame_view());

    // In the typical scenario with frame enabled, we round:
    //   * Upper corners of the frame.
    //   * Lower corners of the client view.
    // But when the frame is overlapped with the client view, for upper corners,
    // both the top (frame) and the bottom (client view) views need to be
    // rounded.
    const bool should_round_client_view_upper_corner =
        !custom_frame_view->GetFrameEnabled() ||
        custom_frame_view->GetFrameOverlapped();

    const float corner_radius_f = corner_radius;
    const gfx::RoundedCornersF root_surface_radii = {
        should_round_client_view_upper_corner ? corner_radius_f : 0,
        should_round_client_view_upper_corner ? corner_radius_f : 0,
        corner_radius_f, corner_radius_f};

    const Surface* root_surface = shell_surface_->root_surface();

    const gfx::RectF bounds =
        root_surface_radii.IsEmpty()
            ? gfx::RectF()
            : gfx::RectF(root_surface->surface_hierarchy_content_bounds());
    shell_surface_->ApplyRoundedCornersToSurfaceTree(bounds,
                                                     root_surface_radii);
  }

 private:
  raw_ptr<ShellSurfaceBase> shell_surface_;
};

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(ShellSurfaceBase* shell_surface)
      : shell_surface_(shell_surface), widget_(shell_surface->GetWidget()) {}

  CustomWindowTargeter(const CustomWindowTargeter&) = delete;
  CustomWindowTargeter& operator=(const CustomWindowTargeter&) = delete;

  ~CustomWindowTargeter() override = default;

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    gfx::Point local_point =
        ConvertEventLocationToWindowCoordinates(window, event);

    if (shell_surface_->shape_dp() &&
        !shell_surface_->shape_dp()->Contains(local_point)) {
      return false;
    }

    if (IsInResizeHandle(window, event, local_point))
      return true;

    Surface* surface = GetShellRootSurface(window);
    if (!surface)
      return false;

    int component =
        widget_->non_client_view()
            ? widget_->non_client_view()->NonClientHitTest(local_point)
            : HTNOWHERE;
    if (component != HTNOWHERE && component != HTCLIENT &&
        component != HTBORDER) {
      return true;
    }

    aura::Window::ConvertPointToTarget(window, surface->window(), &local_point);
    return surface->HitTest(local_point);
  }

 private:
  bool IsInResizeHandle(aura::Window* window,
                        const ui::LocatedEvent& event,
                        const gfx::Point& local_point) const {
    if (window != widget_->GetNativeWindow() ||
        !widget_->widget_delegate()->CanResize()) {
      return false;
    }

    if (!shell_surface_->server_side_resize())
      return false;

    ui::EventTarget* parent =
        static_cast<ui::EventTarget*>(window)->GetParentTarget();
    if (parent) {
      aura::WindowTargeter* parent_targeter =
          static_cast<aura::WindowTargeter*>(parent->GetEventTargeter());

      if (parent_targeter) {
        gfx::Rect mouse_rect;
        gfx::Rect touch_rect;

        if (parent_targeter->GetHitTestRects(window, &mouse_rect,
                                             &touch_rect)) {
          const gfx::Vector2d offset = -window->bounds().OffsetFromOrigin();
          mouse_rect.Offset(offset);
          touch_rect.Offset(offset);
          if (event.IsTouchEvent() || event.IsGestureEvent()
                  ? touch_rect.Contains(local_point)
                  : mouse_rect.Contains(local_point)) {
            return true;
          }
        }
      }
    }
    return false;
  }

  raw_ptr<ShellSurfaceBase> shell_surface_;
  const raw_ptr<views::Widget, DanglingUntriaged> widget_;
};

void CloseAllShellSurfaceTransientChildren(aura::Window* window) {
  // Deleting a window may delete other transient children. Remove other shell
  // surface bases first so they don't get deleted.
  auto list = wm::GetTransientChildren(window);
  for (size_t i = 0; i < list.size(); ++i) {
    if (GetShellSurfaceBaseForWindow(list[i]))
      wm::RemoveTransientChild(window, list[i]);
  }
}

int shell_id = 0;

void ShowSnapPreview(aura::Window* window,
                     chromeos::SnapDirection snap_direction) {
  chromeos::SnapController::Get()->ShowSnapPreview(
      window, snap_direction,
      /*allow_haptic_feedback=*/false);
}

void CommitSnap(aura::Window* window,
                chromeos::SnapDirection snap_direction,
                float snap_ratio) {
  chromeos::SnapController::Get()->CommitSnap(
      window, snap_direction, snap_ratio,
      chromeos::SnapController::SnapRequestSource::
          kFromLacrosSnapButtonOrWindowLayoutMenu);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShellSurfaceBase, public:

ShellSurfaceBase::ShellSurfaceBase(Surface* surface,
                                   const gfx::Point& origin,
                                   bool can_minimize,
                                   int container)
    : SurfaceTreeHost(base::StringPrintf("ExoShellSurfaceHost-%d", shell_id)),
      origin_(origin),
      container_(container),
      can_minimize_(can_minimize) {
  WMHelper::GetInstance()->AddActivationObserver(this);
  WMHelper::GetInstance()->AddTooltipObserver(this);
  surface->AddSurfaceObserver(this);
  SetRootSurface(surface);
  host_window()->Show();
  set_owned_by_client();

  SetCanMinimize(can_minimize_);
  SetCanMaximize(ash::desks_util::IsDeskContainerId(container_));
  SetCanFullscreen(ash::desks_util::IsDeskContainerId(container_));
  SetCanResize(true);
  SetShowTitle(false);
  GetViewAccessibility().SetRole(ax::mojom::Role::kClient);
}

ShellSurfaceBase::~ShellSurfaceBase() {
  // For overlapped frames, a relationship is created between the host window
  // layer and the frame header layer. We need to notify frame header to remove
  // the relationship before host window to destroyed.
  if (frame_overlapped()) {
    auto* frame_header = chromeos::FrameHeader::Get(widget_);
    frame_header->RemoveLayerBeneath();
  }

  // If the surface was TrustedPinned, we have to unpin first as this might have
  // locked down some system functions.
  if (current_pinned_state_ == chromeos::WindowPinType::kTrustedPinned) {
    pending_pinned_state_ = chromeos::WindowPinType::kNone;
    UpdatePinned();
  }

  // Close the overlay in case the window is deleted by the server.
  overlay_widget_.reset();

  // Remove activation observer before hiding widget to prevent it from
  // casuing the configure callback to be called.
  WMHelper::GetInstance()->RemoveActivationObserver(this);

  // Client is gone by now, so don't call callbacks.
  close_callback_.Reset();
  pre_close_callback_.Reset();
  surface_destroyed_callback_.Reset();

  if (widget_) {
    if (has_grab_) {
      widget_->ReleaseCapture();
      WMHelper::GetInstance()->GetCaptureClient()->RemoveObserver(this);
    }
    widget_->GetNativeWindow()->RemoveObserver(this);
    widget_->RemoveObserver(this);
    // Remove transient children which are shell surfaces so they are not
    // automatically destroyed.
    CloseAllShellSurfaceTransientChildren(widget_->GetNativeWindow());
    if (widget_->IsVisible())
      widget_->Hide();
    widget_->CloseNow();
  }
  if (parent_)
    parent_->RemoveObserver(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
  WMHelper::GetInstance()->RemoveTooltipObserver(this);
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void ShellSurfaceBase::Activate() {
  TRACE_EVENT0("exo", "ShellSurfaceBase::Activate");

  if (!widget_ || pending_show_widget_) {
    initially_activated_ = true;
    return;
  }

  if (widget_->IsActive())
    return;

  widget_->Activate();
}

void ShellSurfaceBase::Deactivate() {
  TRACE_EVENT0("exo", "ShellSurfaceBase::Deactivate");

  if (!widget_ || pending_show_widget_) {
    initially_activated_ = false;
    return;
  }

  if (!widget_->IsActive())
    return;

  widget_->Deactivate();
}

void ShellSurfaceBase::SetTitle(const std::u16string& title) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetTitle", "title",
               base::UTF16ToUTF8(title));
  WidgetDelegate::SetTitle(title);

  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->SetFrameSinkDebugLabel(host_window()->GetFrameSinkId(),
                               base::UTF16ToUTF8(title));
}

void ShellSurfaceBase::SetIcon(const gfx::ImageSkia& icon) {
  TRACE_EVENT0("exo", "ShellSurfaceBase::SetIcon");
  WidgetDelegate::SetIcon(ui::ImageModel::FromImageSkia(icon));
}

void ShellSurfaceBase::SetSystemModal(bool system_modal) {
  if (system_modal == system_modal_)
    return;

  bool non_system_modal_window_was_active =
      !system_modal_ && widget_ && widget_->IsActive();

  system_modal_ = system_modal;

  if (widget_) {
    UpdateSystemModal();
    // Deactivate to give the focus back to normal windows.
    if (!system_modal_ && !non_system_modal_window_was_active_) {
      widget_->Deactivate();
    }
  }

  non_system_modal_window_was_active_ = non_system_modal_window_was_active;
}

void ShellSurfaceBase::SetTopInset(int height) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetTopInset", "height", height);
  pending_top_inset_height_ = height;
}

void ShellSurfaceBase::SetWindowCornersRadii(
    const gfx::RoundedCornersF& radii) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetWindowCornerRadii", "radii",
               radii.ToString());
  pending_window_corners_radii_dp_ = radii;
}

void ShellSurfaceBase::SetShadowCornersRadii(
    const gfx::RoundedCornersF& radii) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetShadowCornersRadii", "shadow_radii",
               radii.ToString());
  pending_shadow_corners_radii_dp_ = radii;
}

void ShellSurfaceBase::SetBoundsForShadows(
    const std::optional<gfx::Rect>& shadow_bounds) {
  if (shadow_bounds_ != shadow_bounds) {
    // Set normal shadow bounds.
    shadow_bounds_ = shadow_bounds;
    shadow_bounds_changed_ = true;
    if (widget_ && shadow_bounds) {
      // Set resize shadow bounds and origin.
      const gfx::Rect bounds = shadow_bounds.value();
      const gfx::Point absolute_origin =
          widget_->GetNativeWindow()->bounds().origin();
      const gfx::Rect absolute_bounds =
          gfx::Rect(absolute_origin.x(), absolute_origin.y(), bounds.width(),
                    bounds.height());
      ash::Shell::Get()
          ->resize_shadow_controller()
          ->UpdateResizeShadowBoundsOfWindow(widget_->GetNativeWindow(),
                                             absolute_bounds);
    }
  }
}

void ShellSurfaceBase::UpdateSystemModal() {
  DCHECK(widget_);
  DCHECK_EQ(container_, ash::kShellWindowId_SystemModalContainer);
  widget_->GetNativeWindow()->SetProperty(
      aura::client::kModalKey, system_modal_ ? ui::mojom::ModalType::kSystem
                                             : ui::mojom::ModalType::kNone);
}

void ShellSurfaceBase::UpdateShape() {
  auto* widget_window = widget_->GetNativeWindow();
  if (!widget_window || !widget_window->layer()) {
    return;
  }

  if (!shape_dp_.has_value()) {
    widget_window->layer()->SetAlphaShape(nullptr);
    return;
  }

  // TODO(crbug.com/40276217): The current implementation of window shape must
  // only be used on frameless windows with shadows disabled, otherwise we risk
  // the layer bounds not matching the bounds of the root surface. This needs to
  // be updated such that the shape is applied to the root surface's geometry.
  DCHECK_EQ(frame_type_, SurfaceFrameType::NONE);

  auto shape_rects_dp = std::make_unique<ui::Layer::ShapeRects>();
  for (gfx::Rect rect : shape_dp_.value()) {
    shape_rects_dp->push_back(std::move(rect));
  }

  widget_window->layer()->SetAlphaShape(std::move(shape_rects_dp));
}

void ShellSurfaceBase::SetApplicationId(const char* application_id) {
  // Store the value in |application_id_| in case the window does not exist yet.
  if (application_id)
    application_id_ = std::string(application_id);
  else
    application_id_.reset();

  if (widget_ && widget_->GetNativeWindow()) {
    SetShellApplicationId(widget_->GetNativeWindow(), application_id_);
    WMHelper::AppPropertyResolver::Params params;
    if (application_id_)
      params.app_id = *application_id_;
    if (startup_id_)
      params.startup_id = *startup_id_;
    ui::PropertyHandler& property_handler = *widget_->GetNativeWindow();
    WMHelper::GetInstance()->PopulateAppProperties(params, property_handler);
  }
  if (application_id_) {
    GetViewAccessibility().SetChildTreeNodeAppId(*application_id_);
  } else {
    GetViewAccessibility().RemoveChildTreeNodeAppId();
  }
  this->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                 /* send_native_event */ false);
}

void ShellSurfaceBase::SetStartupId(const char* startup_id) {
  // Store the value in |startup_id_| in case the window does not exist yet.
  if (startup_id)
    startup_id_ = std::string(startup_id);
  else
    startup_id_.reset();

  if (widget_ && widget_->GetNativeWindow())
    SetShellStartupId(widget_->GetNativeWindow(), startup_id_);
}

void ShellSurfaceBase::SetUseImmersiveForFullscreen(bool value) {
  // Store the value in case the window doesn't exist yet.
  immersive_implied_by_fullscreen_ = value;

  if (widget_ && widget_->GetNativeWindow())
    SetShellUseImmersiveForFullscreen(widget_->GetNativeWindow(), value);
}

void ShellSurfaceBase::ShowSnapPreviewToPrimary() {
  ShowSnapPreview(widget_->GetNativeWindow(),
                  chromeos::SnapDirection::kPrimary);
}

void ShellSurfaceBase::ShowSnapPreviewToSecondary() {
  ShowSnapPreview(widget_->GetNativeWindow(),
                  chromeos::SnapDirection::kSecondary);
}

void ShellSurfaceBase::HideSnapPreview() {
  ShowSnapPreview(widget_->GetNativeWindow(), chromeos::SnapDirection::kNone);
}

void ShellSurfaceBase::SetSnapPrimary(float snap_ratio) {
  CommitSnap(widget_->GetNativeWindow(), chromeos::SnapDirection::kPrimary,
             snap_ratio);
}

void ShellSurfaceBase::SetSnapSecondary(float snap_ratio) {
  CommitSnap(widget_->GetNativeWindow(), chromeos::SnapDirection::kSecondary,
             snap_ratio);
}

void ShellSurfaceBase::UnsetSnap() {
  if (widget_ && widget_->GetNativeWindow()) {
    CommitSnap(widget_->GetNativeWindow(), chromeos::SnapDirection::kNone,
               chromeos::kDefaultSnapRatio);
  }
}

void ShellSurfaceBase::SetCanGoBack() {
  if (widget_)
    widget_->GetNativeWindow()->SetProperty(ash::kMinimizeOnBackKey, false);
}

void ShellSurfaceBase::UnsetCanGoBack() {
  if (widget_)
    widget_->GetNativeWindow()->SetProperty(ash::kMinimizeOnBackKey, true);
}

void ShellSurfaceBase::SetPip() {
  if (!widget_) {
    pending_pip_ = true;
    return;
  }
  pending_pip_ = false;

  // Set all the necessary window properties and window state.
  auto* window = widget_->GetNativeWindow();
  window->SetProperty(ash::kWindowPipTypeKey, true);
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);

  if (initial_bounds_)
    return;
  // If no initial bounds is specified, pip windows should start in the bottom
  // right corner of the screen so move |window| to the bottom right of the
  // work area and let the pip positioner move it within the work area.
  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  gfx::Size window_size = window->bounds().size();
  window->SetBoundsInScreen(
      gfx::Rect(display.work_area().bottom_right(), window_size), display);
}

void ShellSurfaceBase::UnsetPip() {
  // Ash does not implement restoring the pip state. Additionally it does not
  // make sense for browser pip window to unset pip since the browser(lacros)
  // creates a separate window for a pip and once pip is not needed,
  // the window is destroyed rather than restoring it to some other state.
  // However, ClientControlledShellSurface(Arc++), has a concept of restoring
  // from pip state and implements UnsetPip.
  NOTIMPLEMENTED();
}

void ShellSurfaceBase::SetFloatToLocation(
    chromeos::FloatStartLocation float_start_location) {
  chromeos::FloatControllerBase::Get()->SetFloat(widget_->GetNativeWindow(),
                                                 float_start_location);
}

void ShellSurfaceBase::MoveToDesk(int desk_index) {
  if (widget_) {
    ash::DesksController::Get()->SendToDeskAtIndex(widget_->GetNativeWindow(),
                                                   desk_index);
  }
}

void ShellSurfaceBase::SetVisibleOnAllWorkspaces() {
  if (widget_)
    widget_->SetVisibleOnAllWorkspaces(true);
}

void ShellSurfaceBase::SetInitialWorkspace(const char* initial_workspace) {
  if (initial_workspace)
    initial_workspace_ = std::string(initial_workspace);
  else
    initial_workspace_.reset();
}

void ShellSurfaceBase::Pin(bool trusted) {
  pending_pinned_state_ = trusted ? chromeos::WindowPinType::kTrustedPinned
                                  : chromeos::WindowPinType::kPinned;
  UpdatePinned();
}

void ShellSurfaceBase::Unpin() {
  // Only need to do something when we have to set a pinned mode.
  if (pending_pinned_state_ == chromeos::WindowPinType::kNone)
    return;

  // Remove any pending pin states which might not have been applied yet.
  pending_pinned_state_ = chromeos::WindowPinType::kNone;
  UpdatePinned();
}

void ShellSurfaceBase::UpdatePinned() {
  if (!widget_) {
    // It is possible to get here before the widget has actually been created.
    // The state will be set once the widget gets created.
    return;
  }
  if (current_pinned_state_ != pending_pinned_state_) {
    auto* window = widget_->GetNativeWindow();
    if (pending_pinned_state_ == chromeos::WindowPinType::kNone) {
      ash::WindowState::Get(window)->Restore();
    } else {
      bool trusted_pinned =
          pending_pinned_state_ == chromeos::WindowPinType::kTrustedPinned;
      ash::window_util::PinWindow(window,
                                  /*trusted=*/trusted_pinned);
    }

    current_pinned_state_ = pending_pinned_state_;
  }
}

void ShellSurfaceBase::UpdateTopInset() {
  if (!widget_) {
    // It is possible to get here before the widget has actually been created.
    // The state will be set once the widget gets created.
    return;
  }

  // Apply new top inset height.
  if (pending_top_inset_height_ != top_inset_height_) {
    widget_->GetNativeWindow()->SetProperty(aura::client::kTopViewInset,
                                            pending_top_inset_height_);
    top_inset_height_ = pending_top_inset_height_;
  }
}

void ShellSurfaceBase::SetChildAxTreeId(ui::AXTreeID child_ax_tree_id) {
  GetViewAccessibility().SetChildTreeID(child_ax_tree_id);
  this->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);
}

void ShellSurfaceBase::SetGeometry(const gfx::Rect& geometry) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetGeometry", "geometry",
               geometry.ToString());

  if (geometry.IsEmpty()) {
    DLOG(WARNING) << "Surface geometry must be non-empty";
    return;
  }
  pending_geometry_ = geometry;
}

void ShellSurfaceBase::SetWindowBounds(const gfx::Rect& bounds) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetWindowBounds", "bounds",
               bounds.ToString());
  if (!widget_) {
    initial_bounds_.emplace(bounds);
    return;
  }

  SecurityDelegate* security = GetSecurityDelegate();
  if (!security) {
    return;
  }

  switch (security->CanSetBounds(widget_->GetNativeWindow())) {
    // Disallowed by default.
    case SecurityDelegate::SetBoundsPolicy::IGNORE:
      break;

    // For selected clients (Borealis) expand the requested bounds to include
    // the decorations, if any.
    //
    // TODO(crbug.com/1261321, b/268395213): Instead, tell clients how large the
    // decorations are, so they can make better decisions.
    case SecurityDelegate::SetBoundsPolicy::ADJUST_IF_DECORATED:
      if (widget_->non_client_view()) {
        gfx::Rect expanded_bounds{
            widget_->non_client_view()->GetWindowBoundsForClientBounds(bounds)};

        // If this expansion pushes the title bar offscreen, push it back
        // onscreen while preserving requested X coordinate, width, and height.
        gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayMatching(bounds)
                                  .work_area();
        if (!work_area.IsEmpty() && expanded_bounds.y() < work_area.y()) {
          expanded_bounds.Offset(0, work_area.y() - expanded_bounds.y());
        }
        widget_->SetBounds(expanded_bounds);
      } else {
        // No decorations, so no adjustment needed.
        widget_->SetBounds(bounds);
      }
      break;

    // Other clients (Lacros) may set bounds, but it's a bug to do so for
    // decorated windows. The chosen way to detect such bugs is a DCHECK.
    //
    // TODO(crbug.com/1261321, b/268395213): Instead, tell clients how large the
    // decorations are, so they can make better decisions.
    case SecurityDelegate::SetBoundsPolicy::DCHECK_IF_DECORATED:
      DCHECK(!frame_enabled());
      widget_->SetBounds(bounds);
      break;
  }
}

void ShellSurfaceBase::SetRestoreInfo(int32_t restore_session_id,
                                      int32_t restore_window_id) {
  // TODO(crbug.com/1327490): Rename restore info variables.
  // Restore information must be set before widget is created.
  DCHECK(!widget_);
  restore_session_id_.emplace(restore_session_id);
  restore_window_id_.emplace(restore_window_id);
  ash::LoginUnlockThroughputRecorder* throughput_recorder =
      ash::Shell::Get()->login_unlock_throughput_recorder();
  throughput_recorder->OnRestoredWindowCreated(restore_window_id);
}

void ShellSurfaceBase::SetRestoreInfoWithWindowIdSource(
    int32_t restore_session_id,
    const std::string& restore_window_id_source) {
  restore_session_id_.emplace(restore_session_id);
  if (!restore_window_id_source.empty())
    restore_window_id_source_.emplace(restore_window_id_source);
}

void ShellSurfaceBase::UnsetFloat() {
  chromeos::FloatControllerBase::Get()->UnsetFloat(widget_->GetNativeWindow());
}

void ShellSurfaceBase::SetDisplay(int64_t display_id) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetDisplay", "display_id", display_id);

  pending_display_id_ = display_id;
}

void ShellSurfaceBase::SetOrigin(const gfx::Point& origin) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetOrigin", "origin",
               origin.ToString());

  origin_ = origin;
}

void ShellSurfaceBase::SetActivatable(bool activatable) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetActivatable", "activatable",
               activatable);

  activatable_ = activatable;
}

void ShellSurfaceBase::SetContainer(int container) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetContainer", "container", container);
  SetContainerInternal(container);
}

void ShellSurfaceBase::SetMaximumSize(const gfx::Size& size) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetMaximumSize", "size",
               size.ToString());
  pending_maximum_size_ = size;
}

void ShellSurfaceBase::SetMinimumSize(const gfx::Size& size) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetMinimumSize", "size",
               size.ToString());

  pending_minimum_size_ = size;
}

void ShellSurfaceBase::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetAspectRatio", "aspect_ratio",
               aspect_ratio.ToString());

  pending_aspect_ratio_ = aspect_ratio;
}

void ShellSurfaceBase::SetCanMinimize(bool can_minimize) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetCanMinimize", "can_minimize",
               can_minimize);

  can_minimize_ = can_minimize;
  WidgetDelegate::SetCanMinimize(!parent_ && can_minimize_);
}

void ShellSurfaceBase::SetPersistable(bool persistable) {
  // This should be called before the widget is created.
  DCHECK(!widget_);

  persistable_ = persistable;
}

void ShellSurfaceBase::SetMenu() {
  is_menu_ = true;
}

void ShellSurfaceBase::DisableMovement() {
  movement_disabled_ = true;
  SetCanResize(false);

  if (widget_)
    widget_->set_movement_disabled(true);
}

void ShellSurfaceBase::UpdateResizability() {
  SetCanResize(CalculateCanResize());
  auto max_size = GetMaximumSize();
  bool max_size_resizability_only = false;
  if (widget_ && widget_->GetNativeWindow()) {
    max_size_resizability_only = widget_->GetNativeWindow()->GetProperty(
        kMaximumSizeForResizabilityOnly);
  }

  // Allow maximizing if the max size is bigger than 32k resolution.
  SetCanMaximize(CanResize() && !parent_ &&
                 ash::desks_util::IsDeskContainerId(container_) &&
                 (max_size.IsEmpty() || max_size_resizability_only));
}

void ShellSurfaceBase::RebindRootSurface(Surface* root_surface,
                                         bool can_minimize,
                                         int container) {
  can_minimize_ = can_minimize;
  container_ = container;
  this->root_surface()->RemoveSurfaceObserver(this);
  root_surface->AddSurfaceObserver(this);
  SetRootSurface(root_surface);
  host_window()->Show();

  // Re-apply window properties to the new root surface.
  auto* window = widget_ ? widget_->GetNativeWindow() : nullptr;
  if (window) {
    // Int properties.
    for (auto* const key :
         {aura::client::kSkipImeProcessing, chromeos::kFrameRestoreLookKey,
          ash::kFrameRateThrottleKey}) {
      if (base::Contains(window->GetAllPropertyKeys(), key)) {
        OnWindowPropertyChanged(window, key,
                                /*old_value(unused)=*/0);
      }
    }
    // Boolean property.
    if (base::Contains(window->GetAllPropertyKeys(),
                       aura::client::kWindowWorkspaceKey)) {
      OnWindowPropertyChanged(window, aura::client::kWindowWorkspaceKey,
                              /*old_value(unused)=*/0);
    }
    if (window->HasFocus())
      root_surface->window()->Focus();
  }

  SetCanMinimize(can_minimize_);
  SetCanMaximize(ash::desks_util::IsDeskContainerId(container_));
  SetCanFullscreen(ash::desks_util::IsDeskContainerId(container_));
  SetCanResize(true);
  SetShowTitle(false);
}

std::unique_ptr<base::trace_event::TracedValue>
ShellSurfaceBase::AsTracedValue() const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("title", base::UTF16ToUTF8(GetWindowTitle()));
  if (GetWidget() && GetWidget()->GetNativeWindow()) {
    const std::string* application_id =
        GetShellApplicationId(GetWidget()->GetNativeWindow());

    if (application_id)
      value->SetString("application_id", *application_id);

    const std::string* startup_id =
        GetShellStartupId(GetWidget()->GetNativeWindow());

    if (startup_id)
      value->SetString("startup_id", *startup_id);
  }
  return value;
}

void ShellSurfaceBase::AddOverlay(OverlayParams&& overlay_params) {
  DCHECK(widget_);
  DCHECK(!overlay_widget_);
  overlay_overlaps_frame_ = overlay_params.overlaps_frame;
  overlay_can_resize_ = std::move(overlay_params.can_resize);

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_CONTROL);
  params.parent = widget_->GetNativeWindow();
  if (overlay_params.translucent)
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  if (overlay_params.focusable)
    params.activatable = views::Widget::InitParams::Activatable::kYes;

  params.delegate = new views::WidgetDelegate();
  params.delegate->SetOwnedByWidget(true);
  params.delegate->SetContentsView(std::move(overlay_params.contents_view));
  params.name = "Overlay";

  overlay_widget_ = std::make_unique<views::Widget>();
  overlay_widget_->Init(std::move(params));
  overlay_widget_->GetNativeWindow()->SetEventTargeter(
      std::make_unique<aura::WindowTargeter>());

  if (overlay_params.corners_radii) {
    ui::Layer* layer = overlay_widget_->GetLayer();
    const gfx::RoundedCornersF& radii = overlay_params.corners_radii.value();
    layer->SetRoundedCornerRadius(radii);
    layer->SetIsFastRoundedCorner(/*enable=*/!radii.IsEmpty());
  }

  overlay_widget_->Show();

  // Setup Focus Traversal.
  overlay_widget_->SetFocusTraversableParentView(this);
  overlay_widget_->SetFocusTraversableParent(
      GetWidget()->GetFocusTraversable());
  SetFocusTraversesOut(true);

  skip_ime_processing_ = GetWidget()->GetNativeWindow()->GetProperty(
      aura::client::kSkipImeProcessing);
  if (skip_ime_processing_) {
    GetWidget()->GetNativeWindow()->SetProperty(
        aura::client::kSkipImeProcessing, false);
  }

  set_bounds_is_dirty(true);
  UpdateWidgetBounds();
  UpdateResizability();
}

void ShellSurfaceBase::RemoveOverlay() {
  overlay_widget_.reset();
  SetFocusTraversesOut(false);
  if (skip_ime_processing_) {
    GetWidget()->GetNativeWindow()->SetProperty(
        aura::client::kSkipImeProcessing, true);
  }
  UpdateResizability();
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurfaceBase::OnSurfaceCommit() {
  // Pause occlusion tracking since we will update a bunch of window properties.
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;

  // SetShadowBounds requires synchronizing shadow bounds with the next frame,
  // so submit the next frame to a new surface and let the host window use the
  // new surface.
  if (shadow_bounds_changed_) {
    AllocateLocalSurfaceId();
  }

  const gfx::Rect old_content_bounds =
      root_surface()->surface_hierarchy_content_bounds();

  root_surface()->CommitSurfaceHierarchy(false);

  set_bounds_is_dirty(bounds_is_dirty() ||
                      old_content_bounds !=
                          root_surface()->surface_hierarchy_content_bounds());

  if (!OnPreWidgetCommit())
    return;

  WillCommit();

  CommitWidget();
  OnPostWidgetCommit();
  SubmitCompositorFrame();
}

bool ShellSurfaceBase::IsInputEnabled(Surface*) const {
  return true;
}

void ShellSurfaceBase::OnSetFrame(SurfaceFrameType frame_type) {
  if (!IsFrameDecorationSupported(frame_type)) {
    DLOG(WARNING)
        << "popup does not support frame decoration other than NONE/SHADOW.";
    return;
  }

  bool frame_type_changed = frame_type_ != frame_type;

  // aura-shell's set_frame, when used with xdg-shell, works iff the frame type
  // or frame colors were specified before firsrt buffer commit. If these are
  // not specified, the widget's layer is set to 'NOT_DRAWN' and the frame can't
  // be drawn. `ClientControlledShellSurface` is not affected.
  if (frame_type_changed && widget_ &&
      widget_->GetNativeWindow()->layer()->type() == ui::LAYER_NOT_DRAWN) {
    if (frame_type != SurfaceFrameType::NONE &&
        frame_type != SurfaceFrameType::SHADOW) {
      DLOG(FATAL)
          << "A shell surface with NOT_DRAWN layer can't support visible frame";
      return;
    }
  }
  bool frame_was_disabled = !frame_enabled();

  frame_type_ = frame_type;
  switch (frame_type) {
    case SurfaceFrameType::NONE:
      shadow_bounds_.reset();
      break;
    case SurfaceFrameType::NORMAL:
    case SurfaceFrameType::AUTOHIDE:
    case SurfaceFrameType::OVERLAY:
    case SurfaceFrameType::OVERLAP:
    case SurfaceFrameType::SHADOW:
      // Initialize the shadow if it didn't exist. Do not reset if
      // the frame type just switched from another enabled type or
      // there is a pending shadow_bounds_ change to avoid overriding
      // a shadow bounds which have been changed and not yet committed.
      if (frame_type_changed &&
          (!shadow_bounds_ || (frame_was_disabled && !shadow_bounds_changed_)))
        shadow_bounds_ = gfx::Rect();
      break;
  }
  if (!widget_)
    return;

  // Override redirect window and popup can request NONE/SHADOW. The shadow
  // will be updated in next commit.
  if (widget_->non_client_view()) {
    CustomFrameView* frame_view =
        static_cast<CustomFrameView*>(widget_->non_client_view()->frame_view());
    if (frame_view->GetFrameEnabled() == frame_enabled() &&
        frame_view->GetFrameOverlapped() == frame_overlapped()) {
      return;
    }

    frame_view->SetFrameEnabled(frame_enabled());
    frame_view->SetShouldPaintHeader(frame_enabled());

    frame_view->SetFrameOverlapped(frame_overlapped());

    auto* frame_header = chromeos::FrameHeader::Get(widget_);
    if (frame_overlapped()) {
      frame_header->AddLayerBeneath(host_window());
    } else {
      frame_header->RemoveLayerBeneath();
    }
  }

  widget_->GetRootView()->DeprecatedLayoutImmediately();
  // TODO(oshima): We probably should wait applying these if the
  // window is animating.
  set_bounds_is_dirty(true);
  UpdateWidgetBounds();
  UpdateHostWindowOrigin();
}

void ShellSurfaceBase::OnSetFrameColors(SkColor active_color,
                                        SkColor inactive_color) {
  has_frame_colors_ = true;
  active_frame_color_ = SkColorSetA(active_color, SK_AlphaOPAQUE);
  inactive_frame_color_ = SkColorSetA(inactive_color, SK_AlphaOPAQUE);
  if (widget_) {
    // Set kTrackDefaultFrameColors to false to prevent clobbering the active
    // and inactive frame colors during theme changes.
    widget_->GetNativeWindow()->SetProperty(chromeos::kTrackDefaultFrameColors,
                                            false);
    widget_->GetNativeWindow()->SetProperty(chromeos::kFrameActiveColorKey,
                                            active_frame_color_);
    widget_->GetNativeWindow()->SetProperty(chromeos::kFrameInactiveColorKey,
                                            inactive_frame_color_);
  }
}

void ShellSurfaceBase::OnSetStartupId(const char* startup_id) {
  SetStartupId(startup_id);
}

void ShellSurfaceBase::OnSetApplicationId(const char* application_id) {
  SetApplicationId(application_id);
}

void ShellSurfaceBase::OnActivationRequested() {
  RequestActivation();
}

void ShellSurfaceBase::RequestActivation() {
  if (!IsReady()) {
    initially_activated_ = true;
    return;
  }
  if (widget_ && GetSecurityDelegate() &&
      GetSecurityDelegate()->CanSelfActivate(widget_->GetNativeWindow())) {
    this->Activate();
  }
}

void ShellSurfaceBase::RequestDeactivation() {
  if (!IsReady()) {
    initially_activated_ = false;
    return;
  }
  if (widget_ && GetSecurityDelegate() && IsReady() &&
      GetSecurityDelegate()->CanSelfActivate(widget_->GetNativeWindow())) {
    this->Deactivate();
  }
}

void ShellSurfaceBase::OnSetServerStartResize() {
  server_side_resize_ = true;
}

bool ShellSurfaceBase::IsReady() const {
  return widget_ && !pending_show_widget_;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void ShellSurfaceBase::OnSurfaceDestroying(Surface* surface) {
  DCHECK_EQ(root_surface(), surface);
  surface->RemoveSurfaceObserver(this);
  SetRootSurface(nullptr);

  overlay_widget_.reset();

  // Hide widget before surface is destroyed. This allows hide animations to
  // run using the current surface contents.
  if (widget_) {
    // Remove transient children which are shell surfaces so they are not
    // automatically hidden.
    CloseAllShellSurfaceTransientChildren(widget_->GetNativeWindow());
    widget_->Hide();
  }

  // Note: In its use in the Wayland server implementation, the surface
  // destroyed callback may destroy the ShellSurface instance. This call needs
  // to be last so that the instance can be destroyed.
  if (!surface_destroyed_callback_.is_null())
    std::move(surface_destroyed_callback_).Run();
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

bool ShellSurfaceBase::OnCloseRequested(
    views::Widget::ClosedReason close_reason) {
  if (!pre_close_callback_.is_null())
    pre_close_callback_.Run();
  // Closing the shell surface is a potentially asynchronous operation, so we
  // will defer actually closing the Widget right now, and come back and call
  // CloseNow() when the callback completes and the shell surface is destroyed
  // (see ~ShellSurfaceBase()).
  if (!close_callback_.is_null())
    close_callback_.Run();
  return false;
}

void ShellSurfaceBase::WindowClosing() {
  SetEnabled(false);
  if (widget_)
    widget_->RemoveObserver(this);
  widget_ = nullptr;
}

views::Widget* ShellSurfaceBase::GetWidget() {
  return widget_;
}

const views::Widget* ShellSurfaceBase::GetWidget() const {
  return widget_;
}

views::View* ShellSurfaceBase::GetContentsView() {
  return this;
}

views::ClientView* ShellSurfaceBase::CreateClientView(views::Widget* widget) {
  return new CustomClientView(widget, this);
}

std::unique_ptr<views::NonClientFrameView>
ShellSurfaceBase::CreateNonClientFrameView(views::Widget* widget) {
  return CreateNonClientFrameViewInternal(widget);
}

bool ShellSurfaceBase::ShouldSaveWindowPlacement() const {
  return !is_popup_ && !movement_disabled_;
}

bool ShellSurfaceBase::WidgetHasHitTestMask() const {
  return true;
}

void ShellSurfaceBase::GetWidgetHitTestMask(SkPath* mask) const {
  // If a window shape is applied set the hit test mask to the boundary path
  // of the masked region.
  if (shape_dp_) {
    shape_dp_->GetBoundaryPath(mask);
    return;
  }

  if (!HasHitTestRegion()) {
    return;
  }
  GetHitTestMask(mask);

  const float scale = GetScale();

  // `mask` should be in the Widget's coordinates, but the above
  // GetHitTestMask() call returns the mask in the root_surface's coordinates.
  // We need to offset the difference.
  auto widget_bounds = widget_->GetWindowBoundsInScreen().origin();
  auto root_surface_bounds =
      root_surface()->window()->GetBoundsInScreen().origin();
  auto offset = root_surface_bounds - widget_bounds.OffsetFromOrigin();

  SkMatrix matrix;
  matrix.setScaleTranslate(
      SkFloatToScalar(1.0f / scale), SkFloatToScalar(1.0f / scale),
      SkIntToScalar(offset.x()), SkIntToScalar(offset.y()));
  mask->transform(matrix);
}

void ShellSurfaceBase::OnCaptureChanged(aura::Window* lost_capture,
                                        aura::Window* gained_capture) {
  if (lost_capture != widget_->GetNativeWindow() || !is_popup_)
    return;

  // If the capture mode is active, do not close the popup to include it in a
  // screenshot.
  if (!views::ViewsDelegate::GetInstance()
           ->ShouldCloseMenuIfMouseCaptureLost()) {
    return;
  }

  WMHelper::GetInstance()->GetCaptureClient()->RemoveObserver(this);

  // Fast return for a simple case: if `lost_capture` is the parent of
  // `gained_capture`, do nothing.
  aura::Window* gained_capture_parent =
      gained_capture ? wm::GetTransientParent(gained_capture) : nullptr;
  if (lost_capture == gained_capture_parent)
    return;

  if (!gained_capture) {
    // If `gained_capture` is nullptr, find the closest ancestor of
    // `lost_capture` that is a popup with grab.
    for (aura::Window* next = wm::GetTransientParent(lost_capture);
         next != nullptr; next = wm::GetTransientParent(next)) {
      if (IsPopupWithGrab(next)) {
        gained_capture = next;
        break;
      }
    }
    // Give capture to the new `gained_capture`.
    if (gained_capture) {
      ShellSurfaceBase* parent_shell_surface =
          GetShellSurfaceBaseForWindow(gained_capture);
      parent_shell_surface->StartCapture();
    }
  }

  // Close any popup that satisfies the following conditions:
  // 1) it has grab, and it is not `gained_capture` or any of its ancestors; or
  // 2) descendants of any popup satisfying (1).
  //
  // Imagine there are the following popups:
  //
  //    popup_e
  //   (no grab)
  //       |
  //    popup_d
  // (has grab; lost_capture)
  //       |
  //    popup_c       popup_b
  //   (no grab) (has grab; gained_capture)
  //         \         /
  //          \       /
  //           popup_a
  //
  // In this case, popup_e and popup_d are the ones to close, in the order
  // from leaf to root.

  // Please note that `gained_capture_ancestors` also includes `gained_capture`.
  base::flat_set<aura::Window*> gained_capture_ancestors;
  for (aura::Window* next = gained_capture; next != nullptr;
       next = wm::GetTransientParent(next)) {
    gained_capture_ancestors.insert(next);
  }

  // BFS to collect all transient windows. The boolean field indicates whether
  // the corresponding window is a popup to be closed.
  std::vector<std::pair<aura::Window*, bool>> all;

  auto is_close_candidate_with_popup_grab =
      [&gained_capture_ancestors](aura::Window* window) {
        return IsPopupWithGrab(window) &&
               !base::Contains(gained_capture_ancestors, window);
      };

  aura::Window* root = wm::GetTransientRoot(lost_capture);
  all.emplace_back(root, is_close_candidate_with_popup_grab(root));

  // Use index instead of iterator because the vector grows during the
  // iteration.
  for (size_t i = 0; i < all.size(); ++i) {
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& children =
        wm::GetTransientChildren(all[i].first);
    for (aura::Window* child : children) {
      const bool to_close =
          all[i].second || is_close_candidate_with_popup_grab(child);
      all.emplace_back(child, to_close);
    }
  }

  // Traverse backwards so that popups are closed in the direction from leaf to
  // root.
  for (auto iter = all.rbegin(); iter != all.rend(); ++iter) {
    if (!iter->second)
      continue;

    ShellSurfaceBase* shell_surface =
        exo::GetShellSurfaceBaseForWindow(iter->first);
    DCHECK(shell_surface);
    shell_surface->widget_->Close();
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver overrides:

void ShellSurfaceBase::OnWidgetClosing(views::Widget* widget) {
  DCHECK(widget_ == widget);
  // To force the widget to close we first disconnect this shell surface from
  // its underlying surface, by asserting to it that the surface destroyed
  // itself. After that, it is safe to call CloseNow() on the widget.
  //
  // TODO(crbug.com/40651062): This only closes the aura/exo pieces, but we
  // should go one level deeper and destroy the wayland stuff. Some options:
  //  - Invoke xkill under-the-hood, which will only work for x11 and won't
  //    work if the container itself is stuck.
  //  - Close the wl connection to the client (i.e. wlkill) this is
  //    problematic with X11 as all of xwayland shares the same client.
  //  - Transitively kill all the wl_resources rooted at this window's
  //    wl_surface, which is not really supported in wayland.
  surface_destroyed_callback_.Reset();
  OnSurfaceDestroying(root_surface());
}

////////////////////////////////////////////////////////////////////////////////
// views::Views overrides:

gfx::Size ShellSurfaceBase::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!geometry_.IsEmpty())
    return geometry_.size();

  // The root surface's content bounds should be used instead of the host window
  // bounds because the host window bounds are not updated until the widget is
  // committed, meaning that if we need to calculate the preferred size before
  // then (e.g. in OnPreWidgetCommit()), then we need to use the root surface's
  // to ensure that we're using the correct bounds' size.
  return root_surface()->surface_hierarchy_content_bounds().size();
}

gfx::Size ShellSurfaceBase::GetMinimumSize() const {
  return requested_minimum_size_.IsEmpty() ? gfx::Size(1, 1)
                                           : requested_minimum_size_;
}

gfx::Size ShellSurfaceBase::GetMaximumSize() const {
  // On ChromeOS, non empty maximum size will make the window
  // non maximizable.
  gfx::Size maximum_size = requested_maximum_size_;
  // Make sure that the max size is already equal to or greater than the min
  // size if set.
  if (!requested_minimum_size_.IsEmpty() &&
      !requested_maximum_size_.IsEmpty()) {
    maximum_size.SetToMax(requested_minimum_size_);
  }
  return maximum_size;
}

views::FocusTraversable* ShellSurfaceBase::GetFocusTraversable() {
  return overlay_widget_.get();
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ShellSurfaceBase::OnWindowDestroying(aura::Window* window) {
  surface_destroyed_callback_.Reset();
  if (!close_callback_.is_null()) {
    close_callback_.Run();
  }

  if (window == parent_)
    SetParentInternal(nullptr);
  window->RemoveObserver(this);
  if (IsShellSurfaceWindow(window) && root_surface()) {
    root_surface()->ThrottleFrameRate(false);
  }
}

void ShellSurfaceBase::OnWindowPropertyChanged(aura::Window* window,
                                               const void* key,
                                               intptr_t old_value) {
  if (!IsShellSurfaceWindow(window) || !root_surface()) {
    return;
  }

  if (key == aura::client::kSkipImeProcessing) {
    SetSkipImeProcessingToDescendentSurfaces(
        window, window->GetProperty(aura::client::kSkipImeProcessing));
  } else if (key == chromeos::kFrameRestoreLookKey) {
    root_surface()->SetFrameLocked(
        window->GetProperty(chromeos::kFrameRestoreLookKey));
  } else if (key == aura::client::kWindowWorkspaceKey) {
    root_surface()->OnDeskChanged(GetWindowDeskStateChanged(window));
  } else if (key == ash::kFrameRateThrottleKey) {
    root_surface()->ThrottleFrameRate(
        window->GetProperty(ash::kFrameRateThrottleKey));
  }
}

void ShellSurfaceBase::OnWindowAddedToRootWindow(aura::Window* window) {
  if (!IsShellSurfaceWindow(window)) {
    return;
  }
  UpdateDisplayOnTree();
}

void ShellSurfaceBase::OnWindowParentChanged(aura::Window* window,
                                             aura::Window* parent) {
  if (!IsShellSurfaceWindow(window) || !root_surface()) {
    return;
  }
  root_surface()->OnDeskChanged(GetWindowDeskStateChanged(window));
}

////////////////////////////////////////////////////////////////////////////////
// wm::ActivationChangeObserver overrides:

void ShellSurfaceBase::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (!widget_)
    return;

  if (overlay_widget_ && overlay_widget_->widget_delegate()->CanActivate()) {
    aura::client::FocusClient* client =
        aura::client::GetFocusClient(widget_->GetNativeWindow());
    client->ResetFocusWithinActiveWindow(overlay_widget_->GetNativeWindow());
  }

  if (gained_active == widget_->GetNativeWindow() ||
      lost_active == widget_->GetNativeWindow()) {
    DCHECK(gained_active != widget_->GetNativeWindow() || CanActivate());
    UpdateShadow();
  }
}

////////////////////////////////////////////////////////////////////////////////
// wm::TooltipObserver overrides:

void ShellSurfaceBase::OnTooltipShown(aura::Window* target,
                                      const std::u16string& text,
                                      const gfx::Rect& bounds) {
  if (root_surface()) {
    root_surface()->OnTooltipShown(text, bounds);
  }
}

void ShellSurfaceBase::OnTooltipHidden(aura::Window* target) {
  if (root_surface()) {
    root_surface()->OnTooltipHidden();
  }
}

// Returns true if surface is currently being resized.
bool ShellSurfaceBase::IsDragged() const {
  if (in_extended_drag_)
    return true;

  if (!widget_)
    return false;

  ash::WindowState* window_state =
      ash::WindowState::Get(widget_->GetNativeWindow());
  return window_state ? window_state->is_dragged() : false;
}

////////////////////////////////////////////////////////////////////////////////
// ui::AcceleratorTarget overrides:

bool ShellSurfaceBase::AcceleratorPressed(const ui::Accelerator& accelerator) {
  for (const auto& entry : kCloseWindowAccelerators) {
    if (ui::Accelerator(entry.keycode, entry.modifiers) == accelerator) {
      OnCloseRequested(views::Widget::ClosedReason::kUnspecified);
      return true;
    }
  }
  return views::View::AcceleratorPressed(accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost:
void ShellSurfaceBase::SetRootSurface(Surface* root_surface) {
  SurfaceTreeHost::SetRootSurface(root_surface);
  if (widget_) {
    SetShellRootSurface(widget_->GetNativeWindow(), root_surface);
  }
}

float ShellSurfaceBase::GetPendingScaleFactor() const {
  if (!host_window()->parent() && !HasDoubleBufferedPendingScaleFactor()) {
    // Before the initial commit, `host_window()` has not been a descendant of
    // the root window yet so we need to fetch the scale factor directly from
    // the pending target display.
    display::Display display;
    if (display::Screen::GetScreen()->GetDisplayWithDisplayId(
            pending_display_id_, &display)) {
      return display.device_scale_factor();
    }
  }
  return SurfaceTreeHost::GetPendingScaleFactor();
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurfaceBase, protected:

void ShellSurfaceBase::CreateShellSurfaceWidget(
    ui::mojom::WindowShowState show_state) {
  DCHECK(GetEnabled());
  DCHECK(!widget_);

  // Sommelier sets the null application id for override redirect windows,
  // which controls its bounds by itself.
  bool emulate_x11_override_redirect =
      !is_popup_ && parent_ && ash::desks_util::IsDeskContainerId(container_) &&
      !application_id_.has_value();

  if (emulate_x11_override_redirect) {
    // override redirect is used for menu, tooltips etc, which should be placed
    // above normal windows, but below lock screen. Specify the container here
    // to avoid using parent_ in params.parent.
    SetContainerInternal(ash::kShellWindowId_ShelfBubbleContainer);
    // X11 override redirect should not be activatable.
    activatable_ = false;
    DisableMovement();
  }

  if (system_modal_)
    SetModalType(ui::mojom::ModalType::kSystem);

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.type = (emulate_x11_override_redirect || is_menu_)
                    ? views::Widget::InitParams::TYPE_MENU
                    : (is_popup_ ? views::Widget::InitParams::TYPE_POPUP
                                 : views::Widget::InitParams::TYPE_WINDOW);

  params.delegate = this;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.show_state = show_state;

  if (initial_z_order_.has_value())
    params.z_order = initial_z_order_.value();

  if (initial_workspace_.has_value()) {
    const std::string kToggleVisibleOnAllWorkspacesValue = "-1";
    if (initial_workspace_ == kToggleVisibleOnAllWorkspacesValue) {
      // If |initial_workspace_| is -1, window is visible on all workspaces.
      params.visible_on_all_workspaces = true;
    } else {
      params.workspace = initial_workspace_.value();
    }
  }

  // Make shell surface a transient child if |parent_| has been set and
  // container_ isn't specified.
  aura::Window* root_window = ash::Shell::GetRootWindowForNewWindows();
  if (ash::desks_util::IsDeskContainerId(container_)) {
    DCHECK_EQ(ash::desks_util::GetActiveDeskContainerId(), container_);
    if (parent_)
      params.parent = parent_;
    else
      params.context = root_window;
  } else {
    params.parent = ash::Shell::GetContainer(root_window, container_);
  }
  if (initial_bounds_)
    params.bounds = *initial_bounds_;
  else
    params.bounds = gfx::Rect(origin_, gfx::Size());

  // This is called before CommitWidget:
  if (pending_display_id_ != display::kInvalidDisplayId) {
    params.display_id = pending_display_id_;
  }

  params.name = base::StringPrintf("ExoShellSurface-%d", shell_id++);

  WMHelper::AppPropertyResolver::Params property_resolver_params;
  if (application_id_)
    property_resolver_params.app_id = *application_id_;
  if (startup_id_)
    property_resolver_params.startup_id = *startup_id_;
  property_resolver_params.for_creation = true;
  WMHelper::GetInstance()->PopulateAppProperties(
      property_resolver_params, params.init_properties_container);

  SetShellApplicationId(&params.init_properties_container, application_id_);
  SetShellRootSurface(&params.init_properties_container, root_surface());
  SetShellStartupId(&params.init_properties_container, startup_id_);

  bool activatable = activatable_;

  // ShellSurfaces in system modal container are only activatable if input
  // region is non-empty. See OnCommitSurface() for more details.
  if (container_ == ash::kShellWindowId_SystemModalContainer)
    activatable &= HasHitTestRegion();
  // Transient child needs to have an application id to be activatable.
  if (parent_)
    activatable &= application_id_.has_value();
  params.activatable = activatable
                           ? views::Widget::InitParams::Activatable::kYes
                           : views::Widget::InitParams::Activatable::kNo;
  if (restore_session_id_) {
    params.init_properties_container.SetProperty(app_restore::kWindowIdKey,
                                                 *restore_session_id_);
  }
  if (restore_window_id_) {
    params.init_properties_container.SetProperty(
        app_restore::kRestoreWindowIdKey, *restore_window_id_);
  }
  if (restore_window_id_source_) {
    params.init_properties_container.SetProperty(
        app_restore::kRestoreWindowIdKey,
        app_restore::FetchRestoreWindowId(*restore_window_id_source_));
    params.init_properties_container.SetProperty(
        app_restore::kAppIdKey, restore_window_id_source_.value());
  }

  params.init_properties_container.SetProperty(wm::kPersistableKey,
                                               persistable_);

  // Restore `params` to those of the saved `restore_window_id_`.
  app_restore::ModifyWidgetParams(params.init_properties_container.GetProperty(
                                      app_restore::kRestoreWindowIdKey),
                                  &params);

  // If app restore specifies the initial bounds, set `initial_bounds_` to it so
  // that shell surface knows the initial bounds is set.
  if (!params.bounds.IsEmpty() && !initial_bounds_)
    initial_bounds_.emplace(params.bounds);

  OverrideInitParams(&params);

  // Note: NativeWidget owns this widget.
  widget_ = new ShellSurfaceWidget;
  widget_->Init(std::move(params));
  widget_->AddObserver(this);

  // As setting the pinned mode may have come in earlier we apply it now.
  UpdatePinned();

  UpdateTopInset();

  aura::Window* window = widget_->GetNativeWindow();
  {
    // AddChild involves propagating a non-1 device_scale_factor to
    // `host_window()`. Changing device_scale_factor this way does not send
    // configure events. So suppress allocation of its LocalSurfaceId.
    viz::ScopedSurfaceIdAllocator scoped_suppression =
        host_window()->GetSurfaceIdAllocator(base::NullCallback());
    window->AddChild(host_window());
  }
  window->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kTargetAndDescendants);
  if (is_menu_) {
    // Sets menu config id to kGroupingPropertyKey if the window is menu.
    window->SetNativeWindowProperty(
        views::TooltipManager::kGroupingPropertyKey,
        reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
  }
  InstallCustomWindowTargeter();

  // TODO(fangzhoug): Consider performing the first shell_surface configure here
  // s.t. there's no gap between the first configure to the point we start
  // observing states of the window. crbug.com/1505583
  // Start tracking changes to window bounds and window state.
  window->AddObserver(this);
  ash::WindowState* window_state = ash::WindowState::Get(window);
  // Skip initializing window state when `window_state` is null.
  // This happesn when the window type is popup.
  if (window_state) {
    InitializeWindowState(window_state);
  }

  SetShellUseImmersiveForFullscreen(window, immersive_implied_by_fullscreen_);

  // Fade visibility animations for non-activatable windows.
  if (!CanActivate()) {
    wm::SetWindowVisibilityAnimationType(
        window, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  }

  // Register close window accelerators.
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  for (const auto& entry : kCloseWindowAccelerators) {
    focus_manager->RegisterAccelerator(
        ui::Accelerator(entry.keycode, entry.modifiers),
        ui::AcceleratorManager::kNormalPriority, this);
  }

  // Show widget next time Commit() is called.
  pending_show_widget_ = true;

  UpdateDisplayOnTree();

  if (frame_type_ != SurfaceFrameType::NONE)
    OnSetFrame(frame_type_);

  if (pending_pip_)
    SetPip();

  root_surface()->OnDeskChanged(GetWindowDeskStateChanged(window));
  root_surface()->OnFullscreenStateChanged(widget_->IsFullscreen());

  WMHelper::GetInstance()->NotifyExoWindowCreated(widget_->GetNativeWindow());
}

bool ShellSurfaceBase::IsShellSurfaceWindow(const aura::Window* window) const {
  return widget_ && window == widget_->GetNativeWindow();
}

ShellSurfaceBase::OverlayParams::OverlayParams(
    std::unique_ptr<views::View> overlay)
    : contents_view(std::move(overlay)) {}
ShellSurfaceBase::OverlayParams::~OverlayParams() = default;

bool ShellSurfaceBase::IsResizing() const {
  ash::WindowState* window_state =
      ash::WindowState::Get(widget_->GetNativeWindow());
  if (!window_state || !window_state->is_dragged())
    return false;
  return window_state->drag_details() &&
         (window_state->drag_details()->bounds_change &
          ash::WindowResizer::kBoundsChange_Resizes);
}

gfx::Rect ShellSurfaceBase::ComputeAdjustedBounds(
    const gfx::Rect& bounds) const {
  return bounds;
}

void ShellSurfaceBase::UpdateWidgetBounds() {
  DCHECK(widget_);
  std::optional<gfx::Rect> bounds = GetWidgetBounds();
  if (!bounds) {
    return;
  }

  ash::WindowState* window_state =
      ash::WindowState::Get(widget_->GetNativeWindow());
  gfx::Rect adjusted_bounds = ComputeAdjustedBounds(*bounds);

  bool should_update_widget_bounds = bounds_is_dirty() ||
                                     adjusted_bounds != *bounds ||
                                     (window_state && window_state->IsPip());

  set_bounds_is_dirty(false);

  if (!should_update_widget_bounds) {
    return;
  }

  if (overlay_widget_) {
    gfx::Rect content_bounds(adjusted_bounds.size());
    int height = 0;
    if (!overlay_overlaps_frame_ && frame_enabled()) {
      auto* frame_view = static_cast<const ash::NonClientFrameViewAsh*>(
          widget_->non_client_view()->frame_view());
      height = frame_view->NonClientTopBorderHeight();
    }
    content_bounds.set_height(content_bounds.height() - height);
    content_bounds.set_y(height);
    overlay_widget_->SetBounds(content_bounds);
  }

  aura::Window* window = widget_->GetNativeWindow();
  // Return early if the shell is currently managing the bounds of the widget.
  if (window_state && !window_state->allow_set_bounds_direct()) {
    // 1) When a window is either maximized/fullscreen/pinned.
    if (window_state->IsMaximizedOrFullscreenOrPinned())
      return;
    // 2) When a window is snapped.
    if (window_state->IsSnapped())
      return;
    // 3) When a window is being interactively resized.
    if (IsResizing())
      return;
    // 4) When a window's bounds are being animated.
    if (window->layer()->GetAnimator()->IsAnimatingProperty(
            ui::LayerAnimationElement::BOUNDS))
      return;
  }

  SetWidgetBounds(adjusted_bounds, adjusted_bounds != *bounds);
}

void ShellSurfaceBase::UpdateHostWindowOrigin() {
  // There's an animation happening on cloned layers, `host_window()` layer may
  // be "ahead" of client's commit_target_layer. Do not update its origin,
  // instead, rely on SurfaceLayer stretching until the client catches up.
  if (GetCommitTargetLayer() != host_window()->layer()) {
    return;
  }
  gfx::Point origin = GetClientViewBounds().origin();

  origin += GetSurfaceOrigin().OffsetFromOrigin();
  // As `origin` is in DP here, it eventually needs to be converted to pixels.
  // We need to make subpixel adjustment so the `origin` in pixel coordinates
  // will align exactly to a pixel boundary. Here, we calculate the closest
  // pixel boundary by converting to pixels and rounding the original DP value.
  // Note that this shouldn't take `scaled_root_origin` into account as its
  // original value is in pixels and it needs a different type of subpixel
  // adjustment (i.e. preserving the original pixel distance between two
  // points).
  const gfx::Vector2dF surface_origin_subpixel_offset =
      ScaleVector2d(ToRoundedVector2d(ScaleVector2d(origin.OffsetFromOrigin(),
                                                    GetScaleFactor())),
                    1.f / GetScaleFactor()) -
      origin.OffsetFromOrigin();

  const gfx::Vector2dF root_surface_origin_dp = ScaleVector2d(
      root_surface_origin_pixel().OffsetFromOrigin(), 1.f / GetScaleFactor());
  origin -= ToFlooredVector2d(root_surface_origin_dp);
  // Subpixel offset used to adjust the offset of `root_origin` so it will
  // exactly match the original value in pixels.
  const gfx::Vector2dF root_surface_origin_subpixel_offset =
      ToFlooredVector2d(root_surface_origin_dp) - root_surface_origin_dp;

  // Two offsets can be simply added together because
  // `surface_origin_subpixel_offset` is used for shifting the origin on a pixel
  // boundary while `root_surface_origin_subpixel_offset` just ensures that the
  // root surface origin stays the same value in pixel while scrolling when a
  // sub surface moves (e.g. by scrolling) but the actual value it's preserving
  // doesn't matter.
  host_window()->layer()->SetSubpixelPositionOffset(
      surface_origin_subpixel_offset + root_surface_origin_subpixel_offset);

  if (origin != host_window()->bounds().origin()) {
    AllocateLocalSurfaceId();
  }

  gfx::Rect surface_bounds(origin, host_window()->bounds().size());
  if (host_window()->bounds() == surface_bounds)
    return;
  // This may not be necessary
  set_bounds_is_dirty(true);
  host_window()->SetBounds(surface_bounds);
  UpdateHostWindowOpaqueRegion();
}

void ShellSurfaceBase::UpdateShadow() {
  if (!widget_ || !root_surface())
    return;

  aura::Window* window = widget_->GetNativeWindow();

  // Window shadows should be disabled if a window shape has been set.
  //
  // Or if `host_window()`'s layer is not commit_target_layer, `shadow_bounds_`
  // committed by the client should not go to current `widget_`'s shadow, but to
  // the old widget's shadow prior to layer clone. Don't show the new shadow for
  // now.
  // TODO(crbug.com/40285156): Find the old widget's shadow layer and update it,
  // and maybe show new widget's shadow by predicting its dimensions.
  int shadow_elevation = wm::kShadowElevationDefault;
  if (!shadow_bounds_ || shape_dp_.has_value() ||
      GetCommitTargetLayer() != host_window()->layer()) {
    shadow_elevation = wm::kShadowElevationNone;
  } else if (frame_type_ == SurfaceFrameType::SHADOW && is_popup_) {
    shadow_elevation = wm::kShadowElevationMenuOrTooltip;
  }
  wm::SetShadowElevation(window, shadow_elevation);

  // A window may not have a shadow object if the window was created in
  // maximized/fullscreen state.
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);
  if (shadow && shadow_elevation != wm::kShadowElevationNone) {
    gfx::Rect shadow_bounds = GetShadowBounds();
    gfx::Point origin = GetClientViewBounds().origin();

    if (!window->GetProperty(aura::client::kUseWindowBoundsForShadow)) {
      origin += GetSurfaceOrigin().OffsetFromOrigin();
      if (origin.x() != 0 || origin.y() != 0) {
        shadow_bounds.set_origin(origin);
        if (widget_) {
          gfx::Point widget_origin_in_root =
              widget_->GetNativeWindow()->bounds().origin();
          origin += ToFlooredVector2d(
              ScaleVector2d(gfx::Vector2d(widget_origin_in_root.x(),
                                          widget_origin_in_root.y()),
                            1.f / GetScale()));
          gfx::Rect bounds = geometry_;
          bounds.set_origin(origin);
          ash::Shell::Get()
              ->resize_shadow_controller()
              ->UpdateResizeShadowBoundsOfWindow(widget_->GetNativeWindow(),
                                                 bounds);
        }
      }
    }

    shadow->SetContentBounds(shadow_bounds);

    // Surfaces that can't be activated are usually menus and tooltips. Use a
    // small style shadow for them.
    if (!CanActivate())
      shadow->SetElevation(wm::kShadowElevationMenuOrTooltip);

    UpdateShadowRoundedCorners();
  }

  if (window->layer()->type() == ui::LAYER_NOT_DRAWN) {
    DCHECK(!window->GetProperty(chromeos::kWindowManagerManagesOpacityKey));

    // Snapped window should not be opaque because it can be drag-resized, in
    // which case the widget's window can be exposed while waiting for
    // configure_ack + commit.
    bool window_is_opaque = widget_->IsFullscreen() || widget_->IsMaximized();
    window->SetTransparent(!window_is_opaque);
    if (root_surface()->FillsBoundsOpaquely()) {
      // Manually control occlusion, but do not make the window
      // opaque as the host window may not be at the same size unless the
      // window state is either in fullscreen or maximized.
      window->SetOpaqueRegionsForOcclusion(
          {gfx::Rect(window->bounds().size())});
    } else {
      window->SetOpaqueRegionsForOcclusion({});
    }
  }
}

void ShellSurfaceBase::UpdateShadowRoundedCorners() {
  if (!widget_) {
    return;
  }

  shadow_corners_radii_dp_ = pending_shadow_corners_radii_dp_;

  aura::Window* window = widget_->GetNativeWindow();
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);

  if (!shadow) {
    return;
  }

  gfx::RoundedCornersF shadow_radii;

  const ash::WindowState* window_state = ash::WindowState::Get(window);
  if (window_state && window_state->IsPip()) {
    shadow_radii = gfx::RoundedCornersF(chromeos::kPipRoundedCornerRadius);
  } else if (chromeos::features::IsRoundedWindowsEnabled() &&
             (shadow_corners_radii_dp_.has_value() ||
              window_corners_radii_dp_.has_value())) {
    // For backward version compatibility, fallback to use the window radii if
    // the shadow radii is not specified.
    // TODO(crbug.com/40256581): Revisit once all the clients have migrated.
    shadow_radii = shadow_corners_radii_dp_.value_or(
        window_corners_radii_dp_.value_or(gfx::RoundedCornersF()));
  }

  // TODO(crbug.com/40256581): Support shadow with variable radius corners.
  shadow->SetRoundedCornerRadius(shadow_radii.upper_left());
}

void ShellSurfaceBase::UpdateFrameType() {
  // Nothing to do here for now as frame type is updated immediately in
  // OnSetFrame() by default.
}

void ShellSurfaceBase::UpdateWindowRoundedCorners() {
  // If non_client_view is not available, it means that widget_ is neither a
  // normal window or a bubble. Therefore it should not have any decorations
  // including a rounded window.
  if (!widget_ || !widget_->non_client_view()) {
    DCHECK(widget_ && !pending_window_corners_radii_dp_);
    // It is possible to get here before the widget has actually been created.
    // The state will be set once the widget gets created.
    return;
  }

  window_corners_radii_dp_ = pending_window_corners_radii_dp_;
  widget_->non_client_view()->frame_view()->UpdateWindowRoundedCorners();
}

gfx::Rect ShellSurfaceBase::GetVisibleBounds() const {
  // Use |geometry_| if set, otherwise use the visual bounds of the surface.
  if (geometry_.IsEmpty()) {
    gfx::Size size;
    if (root_surface()) {
      float int_part;
      DCHECK(std::modf(root_surface()->content_size().width(), &int_part) ==
                 0.0f &&
             std::modf(root_surface()->content_size().height(), &int_part) ==
                 0.0f);
      size = gfx::ToCeiledSize(root_surface()->content_size());
      if (client_submits_surfaces_in_pixel_coordinates()) {
        float dsf = host_window()->layer()->device_scale_factor();
        size = gfx::ScaleToRoundedSize(size, 1.0f / dsf);
      }
    }
    return gfx::Rect(size);
  }

  return geometry_;
}

gfx::Rect ShellSurfaceBase::GetClientViewBounds() const {
  return (widget_->non_client_view() && !frame_overlapped())
             ? widget_->non_client_view()
                   ->frame_view()
                   ->GetBoundsForClientView()
             // When frame is overlapped with client area, window bounds is the
             // same as client bounds.
             : gfx::Rect(widget_->GetWindowBoundsInScreen().size());
}

gfx::Rect ShellSurfaceBase::GetWidgetBoundsFromVisibleBounds() const {
  auto visible_bounds = GetVisibleBounds();
  return widget_->non_client_view()
             ? widget_->non_client_view()->GetWindowBoundsForClientBounds(
                   visible_bounds)
             : visible_bounds;
}

gfx::Rect ShellSurfaceBase::GetShadowBounds() const {
  return shadow_bounds_->IsEmpty()
             ? gfx::Rect(widget_->GetNativeWindow()->bounds().size())
             : gfx::ScaleToEnclosedRect(*shadow_bounds_, 1.f / GetScale());
}

void ShellSurfaceBase::InstallCustomWindowTargeter() {
  aura::Window* window = widget_->GetNativeWindow();
  window->SetEventTargeter(std::make_unique<CustomWindowTargeter>(this));
}

std::unique_ptr<views::NonClientFrameView>
ShellSurfaceBase::CreateNonClientFrameViewInternal(views::Widget* widget) {
  aura::Window* window = widget_->GetNativeWindow();
  // ShellSurfaces always use immersive mode.
  window->SetProperty(chromeos::kImmersiveIsActive, true);
  ash::WindowState* window_state = ash::WindowState::Get(window);
  if (!frame_enabled() && window_state && !window_state->HasDelegate()) {
    window_state->SetDelegate(std::make_unique<CustomWindowStateDelegate>());
  }
  auto frame_view =
      std::make_unique<CustomFrameView>(widget, this, frame_enabled());
  if (has_frame_colors_)
    frame_view->SetFrameColors(active_frame_color_, inactive_frame_color_);
  return frame_view;
}

bool ShellSurfaceBase::ShouldExitFullscreenFromRestoreOrMaximized() {
  if (widget_ && widget_->GetNativeWindow()) {
    return widget_->GetNativeWindow()->GetProperty(
        kRestoreOrMaximizeExitsFullscreen);
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurfaceBase, private:

float ShellSurfaceBase::GetScale() const {
  return 1.f;
}

void ShellSurfaceBase::StartCapture() {
  widget_->set_auto_release_capture(false);
  WMHelper::GetInstance()->GetCaptureClient()->AddObserver(this);
  // Just capture on the window.
  widget_->SetCapture(nullptr /* view */);
}

void ShellSurfaceBase::OnPostWidgetCommit() {
  // |shadow_bounds_changed_| represents whether |shadow_bounds_| has changed
  // since the last commit, but as UpdateShadow() can be called multiple times
  // in a single commit process, we need to ensure that it's not reset halfway
  // in the current commit by resetting it here.
  shadow_bounds_changed_ = false;

  UpdateTopInset();
}

void ShellSurfaceBase::ShowWidget(bool activate) {
  if (activate) {
    // Widget will minimize itself if the initial state is minimized.
    widget_->Show();
  } else {
    widget_->ShowInactive();
  }
}

void ShellSurfaceBase::SetContainerInternal(int container) {
  container_ = container;
  WidgetDelegate::SetCanMaximize(
      !parent_ && ash::desks_util::IsDeskContainerId(container_));
  WidgetDelegate::SetCanFullscreen(
      !parent_ && ash::desks_util::IsDeskContainerId(container_));
  if (widget_)
    widget_->OnSizeConstraintsChanged();
}

void ShellSurfaceBase::SetParentInternal(aura::Window* parent) {
  parent_ = parent;
  WidgetDelegate::SetCanMinimize(!parent_ && can_minimize_);
  UpdateResizability();
  if (widget_)
    widget_->OnSizeConstraintsChanged();
}

bool ShellSurfaceBase::CalculateCanResize() const {
  if (overlay_widget_ && overlay_can_resize_.has_value())
    return *overlay_can_resize_;
  return !movement_disabled_ && GetCanResizeFromSizeConstraints();
}

void ShellSurfaceBase::CommitWidget() {
  bool size_constraint_changed =
      requested_minimum_size_ != pending_minimum_size_ ||
      requested_maximum_size_ != pending_maximum_size_;
  set_bounds_is_dirty(
      bounds_is_dirty() || origin_ != pending_geometry_.origin() ||
      geometry_ != pending_geometry_ || display_id_ != pending_display_id_ ||
      size_constraint_changed);

  // Apply new window geometry.
  geometry_ = pending_geometry_;
  display_id_ = pending_display_id_;
  shape_dp_ = pending_shape_dp_;

  // Apply new minimum/maximum size.
  requested_minimum_size_ = pending_minimum_size_;
  requested_maximum_size_ = pending_maximum_size_;
  UpdateResizability();

  if (!widget_)
    return;

  if (!pending_aspect_ratio_.IsEmpty()) {
    widget_->SetAspectRatio(pending_aspect_ratio_);
  } else if (widget_->GetNativeWindow()) {
    // TODO(yoshiki): Move the logic to clear aspect ratio into view::Widget.
    widget_->GetNativeWindow()->ClearProperty(aura::client::kAspectRatio);
  }

  // The calling order matters. The frame type has to be updated before
  // calculating the bounds because the bounds computation depends on the frame
  // type (e.g. caption height).
  UpdateFrameType();
  UpdateWidgetBounds();
  UpdateSurfaceLayerSizeAndRootSurfaceOrigin();

  // System modal container is used by clients to implement overlay
  // windows using a single ShellSurface instance.  If hit-test
  // region is empty, then it is non interactive window and won't be
  // activated.
  if (container_ == ash::kShellWindowId_SystemModalContainer) {
    // Prevent window from being activated when hit test region is empty.
    bool activatable = activatable_ && HasHitTestRegion();
    if (activatable != CanActivate()) {
      SetCanActivate(activatable);
      // Activate or deactivate window if activation state changed.
      if (activatable) {
        // Automatically activate only if the window is modal.
        // Non modal window should be activated by a user action.
        // TODO(oshima): Non modal system window does not have an associated
        // task ID, and as a result, it cannot be activated from client side.
        // Fix this (b/65460424) and remove this if condition.
        if (system_modal_)
          wm::ActivateWindow(widget_->GetNativeWindow());
      } else if (widget_->IsActive()) {
        wm::DeactivateWindow(widget_->GetNativeWindow());
      }
    }
  }

  UpdateHostWindowOrigin();
  UpdateShape();

  gfx::Rect bounds = geometry_;
  if (!bounds.IsEmpty() && !widget_->GetNativeWindow()->GetProperty(
                               aura::client::kUseWindowBoundsForShadow)) {
    SetBoundsForShadows(std::make_optional(bounds));
  }

  // The calling order matters. Updated window radius is need to correctly
  // update the radius of the shadow.
  UpdateWindowRoundedCorners();
  UpdateShadow();

  // Don't show yet if the shell surface doesn't have content.
  bool should_show = !host_window()->bounds().IsEmpty();

  // Do not layout the window if the position should not be controlled by window
  // manager. (popup, emulating x11 override direct, or requested not to move)
  if (is_popup_ || movement_disabled_)
    needs_layout_on_show_ = false;

  // Do not center if the initial bounds is set.
  if (initial_bounds_)
    needs_layout_on_show_ = false;

  // Show widget if needed.
  if (pending_show_widget_ && should_show) {
    DCHECK(!widget_->IsClosed());
    DCHECK(!widget_->IsVisible());
    pending_show_widget_ = false;

    auto* window = widget_->GetNativeWindow();
    auto* window_state = ash::WindowState::Get(window);

    // TODO(oshima): This should be set to the
    // `views::Widget::InitParams.bounds`
    if (window_state && window_state->IsMaximizedOrFullscreenOrPinned() &&
        (!initial_bounds_ || initial_bounds_->IsEmpty())) {
      gfx::Size current_content_size = CalculatePreferredSize({});
      gfx::Rect restore_bounds = display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(window)
                                     .work_area();
      if (!current_content_size.IsEmpty())
        restore_bounds.ClampToCenteredSize(current_content_size);

      window_state->SetRestoreBoundsInScreen(restore_bounds);
    }

    // TODO(crbug.com/40212799): Hook this up with the WM's window positioning
    // logic.
    if (needs_layout_on_show_) {
      widget_->CenterWindow(GetWidgetBoundsFromVisibleBounds().size());
      needs_layout_on_show_ = false;
    }

    if (restore_window_id_.has_value()) {
      ash::LoginUnlockThroughputRecorder* throughput_recorder =
          ash::Shell::Get()->login_unlock_throughput_recorder();

      aura::Window* root_window = host_window()->GetRootWindow();
      if (root_window) {
        ui::Compositor* compositor = root_window->layer()->GetCompositor();
        throughput_recorder->OnBeforeRestoredWindowShown(
            restore_window_id_.value(), compositor);
      }
    }

    ShowWidget(initially_activated_);

    if (has_grab_)
      StartCapture();

    if (container_ == ash::kShellWindowId_SystemModalContainer)
      UpdateSystemModal();
  }

  if (size_constraint_changed)
    widget_->OnSizeConstraintsChanged();
}

bool ShellSurfaceBase::IsFrameDecorationSupported(SurfaceFrameType frame_type) {
  if (!is_popup_)
    return true;

  // Popup doesn't support frame types other than NONE/SHADOW.
  return frame_type == SurfaceFrameType::SHADOW ||
         frame_type == SurfaceFrameType::NONE;
}

void ShellSurfaceBase::SetOrientationLock(
    chromeos::OrientationType orientation_lock) {
  TRACE_EVENT1("exo", "ShellSurfaceBase::SetOrientationLock",
               "orientation_lock", static_cast<int>(orientation_lock));

  if (!widget_) {
    initial_orientation_lock_ = orientation_lock;
    return;
  }

  ash::Shell* shell = ash::Shell::Get();
  shell->screen_orientation_controller()->LockOrientationForWindow(
      widget_->GetNativeWindow(), orientation_lock);
}

void ShellSurfaceBase::SetZOrder(ui::ZOrderLevel z_order) {
  // If there is already a widget, we can immediately set its z order.
  if (widget_) {
    widget_->SetZOrderLevel(z_order);
    return;
  }

  // Otherwise, we want to save `z_order` for when `widget_` is initialized.
  initial_z_order_ = z_order;
}

void ShellSurfaceBase::SetShape(std::optional<cc::Region> shape) {
  if (!shape) {
    pending_shape_dp_.reset();
    return;
  }

  if (frame_enabled()) {
    LOG(ERROR) << "SetShape() is not supported for windows with frame enabled.";
    return;
  }

  // SetShape() may be called some time after a window has been created. In case
  // server_side_resize_ has been set we disable it here.
  server_side_resize_ = false;

  // Although window shape is only supported for frameless windows we must also
  // ensure window shadows are disabled as shadows can contribute to the widget
  // window's layer bounds.
  // TODO(crbug.com/40276217): This will not be necessary once the
  // implementation is updated to use the root surface's geometry.
  OnSetFrame(SurfaceFrameType::NONE);

  pending_shape_dp_ = std::move(shape);
}

// static
bool ShellSurfaceBase::IsPopupWithGrab(aura::Window* window) {
  ShellSurfaceBase* shell_surface = exo::GetShellSurfaceBaseForWindow(window);
  if (shell_surface && shell_surface->has_grab_) {
    DCHECK(shell_surface->is_popup_);
    return true;
  }

  return false;
}

}  // namespace exo
