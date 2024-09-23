// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_linux.h"

#include <optional>
#include <utility>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_layout_linux.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/linux/linux_ui.h"
#include "ui/native_theme/native_theme.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/platform_window/extensions/x11_extension.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace {

#if defined(USE_DBUS_MENU)
bool CreateGlobalMenuBar() {
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .supports_global_application_menus;
}
#endif  // defined(USE_DBUS_MENU)

std::unordered_set<std::string>& SentStartupIds() {
  static base::NoDestructor<std::unordered_set<std::string>> sent_startup_ids;
  return *sent_startup_ids;
}

// Returns the event source for the active tab drag session.
std::optional<ui::mojom::DragEventSource> GetCurrentTabDragEventSource() {
  if (auto* source_context = TabDragController::GetSourceContext()) {
    if (auto* drag_controller = source_context->GetDragController()) {
      return drag_controller->event_source();
    }
  }
  return std::nullopt;
}

bool IsShowingFrame(bool use_custom_frame,
                    ui::PlatformWindowState window_state) {
  return use_custom_frame &&
         window_state != ui::PlatformWindowState::kMaximized &&
         window_state != ui::PlatformWindowState::kMinimized &&
         !ui::IsPlatformWindowStateFullscreen(window_state);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux, public:

BrowserDesktopWindowTreeHostLinux::BrowserDesktopWindowTreeHostLinux(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostLinux(native_widget_delegate,
                                 desktop_native_widget_aura),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
  native_frame_ = static_cast<DesktopBrowserFrameAuraLinux*>(
      browser_frame->native_browser_frame());
  native_frame_->set_host(this);

  browser_frame->set_frame_type(browser_frame->UseCustomFrame()
                                    ? views::Widget::FrameType::kForceCustom
                                    : views::Widget::FrameType::kForceNative);

  theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    scale_observation_.Observe(linux_ui);
  }
}

BrowserDesktopWindowTreeHostLinux::~BrowserDesktopWindowTreeHostLinux() {
  native_frame_->set_host(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     BrowserDesktopWindowTreeHost implementation:

void BrowserDesktopWindowTreeHostLinux::AddAdditionalInitProperties(
    const views::Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {
  views::DesktopWindowTreeHostLinux::AddAdditionalInitProperties(params,
                                                                 properties);

  auto* profile = browser_view_->browser()->profile();
  const auto* linux_ui_theme = ui::LinuxUiTheme::GetForProfile(profile);
  properties->prefer_dark_theme =
      linux_ui_theme && linux_ui_theme->PreferDarkTheme();
}

views::DesktopWindowTreeHost*
BrowserDesktopWindowTreeHostLinux::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostLinux::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostLinux::UsesNativeSystemMenu() const {
  return false;
}

void BrowserDesktopWindowTreeHostLinux::FrameTypeChanged() {
  DesktopWindowTreeHostPlatform::FrameTypeChanged();
  UpdateFrameHints();
}

bool BrowserDesktopWindowTreeHostLinux::SupportsMouseLock() {
  auto* wayland_extension = ui::GetWaylandToplevelExtension(*platform_window());
  if (!wayland_extension) {
    return false;
  }

  return wayland_extension->SupportsPointerLock();
}

void BrowserDesktopWindowTreeHostLinux::LockMouse(aura::Window* window) {
  DesktopWindowTreeHostLinux::LockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension =
        ui::GetWaylandToplevelExtension(*platform_window());
    wayland_extension->LockPointer(true /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLinux::UnlockMouse(aura::Window* window) {
  DesktopWindowTreeHostLinux::UnlockMouse(window);

  if (SupportsMouseLock()) {
    auto* wayland_extension =
        ui::GetWaylandToplevelExtension(*platform_window());
    wayland_extension->LockPointer(false /*enabled*/);
  }
}

void BrowserDesktopWindowTreeHostLinux::TabDraggingKindChanged(
    TabDragKind tab_drag_kind) {
  // If there's no tabs left, the browser window is about to close, so don't
  // call SetOverrideRedirect() to prevent the window from flashing.
  if (!browser_view_->tabstrip()->GetModelCount()) {
    return;
  }

  auto* x11_extension = GetX11Extension();
  if (x11_extension && x11_extension->IsWmTiling() &&
      x11_extension->CanResetOverrideRedirect()) {
    bool was_dragging_window =
        browser_frame_->tab_drag_kind() == TabDragKind::kAllTabs;
    bool is_dragging_window = tab_drag_kind == TabDragKind::kAllTabs;
    if (is_dragging_window != was_dragging_window) {
      x11_extension->SetOverrideRedirect(is_dragging_window);
    }
  }

  if (auto* wayland_extension =
          ui::GetWaylandToplevelExtension(*platform_window())) {
    if (tab_drag_kind != TabDragKind::kNone) {
      if (auto event_source = GetCurrentTabDragEventSource()) {
        const auto allow_system_drag = base::FeatureList::IsEnabled(
            features::kAllowWindowDragUsingSystemDragDrop);
        wayland_extension->StartWindowDraggingSessionIfNeeded(
            *event_source, allow_system_drag);
      }
    }
  }
}

bool BrowserDesktopWindowTreeHostLinux::SupportsClientFrameShadow() const {
  return platform_window()->CanSetDecorationInsets() &&
         views::Widget::IsWindowCompositingSupported();
}

void BrowserDesktopWindowTreeHostLinux::UpdateFrameHints() {
  auto* window = platform_window();
  auto window_state = window->GetPlatformWindowState();
  float scale = device_scale_factor();
  auto* view =
      static_cast<BrowserNonClientFrameView*>(browser_frame_->GetFrameView());
  const gfx::Size widget_size =
      view->GetWidget()->GetWindowBoundsInScreen().size();

  if (SupportsClientFrameShadow()) {
    auto insets = CalculateInsetsInDIP(window_state);
    if (insets.IsEmpty()) {
      window->SetInputRegion(std::nullopt);
    } else {
      gfx::Rect input_bounds(widget_size);
      input_bounds.Inset(insets - view->GetInputInsets());
      input_bounds = gfx::ScaleToEnclosingRect(input_bounds, scale);
      window->SetInputRegion(
          std::optional<std::vector<gfx::Rect>>({input_bounds}));
    }
  }

  if (ui::OzonePlatform::GetInstance()->IsWindowCompositingSupported()) {
    // Set the opaque region.
    std::vector<gfx::Rect> opaque_region;
    if (IsShowingFrame(browser_frame_->native_browser_frame()->UseCustomFrame(),
                       window_state)) {
      // The opaque region is a list of rectangles that contain only fully
      // opaque pixels of the window.  We need to convert the clipping
      // rounded-rect into this format.
      SkRRect rrect = view->GetRestoredClipRegion();
      gfx::RectF rectf = gfx::SkRectToRectF(rrect.rect());
      rectf.Scale(scale);
      // It is acceptable to omit some pixels that are opaque, but the region
      // must not include any translucent pixels.  Therefore, we must
      // conservatively scale to the enclosed rectangle.
      gfx::Rect rect = gfx::ToEnclosedRect(rectf);

      // Create the initial region from the clipping rectangle without rounded
      // corners.
      SkRegion region(gfx::RectToSkIRect(rect));

      // Now subtract out the small rectangles that cover the corners.
      struct {
        SkRRect::Corner corner;
        bool left;
        bool upper;
      } kCorners[] = {
          {SkRRect::kUpperLeft_Corner, true, true},
          {SkRRect::kUpperRight_Corner, false, true},
          {SkRRect::kLowerLeft_Corner, true, false},
          {SkRRect::kLowerRight_Corner, false, false},
      };
      for (const auto& corner : kCorners) {
        auto radii = rrect.radii(corner.corner);
        auto rx = std::ceil(scale * radii.x());
        auto ry = std::ceil(scale * radii.y());
        auto corner_rect = SkIRect::MakeXYWH(
            corner.left ? rect.x() : rect.right() - rx,
            corner.upper ? rect.y() : rect.bottom() - ry, rx, ry);
        region.op(corner_rect, SkRegion::kDifference_Op);
      }

      auto translucent_top_area_rect = SkIRect::MakeXYWH(
          rect.x(), rect.y(), rect.width(),
          std::ceil(view->GetTranslucentTopAreaHeight() * scale - rect.y()));
      region.op(translucent_top_area_rect, SkRegion::kDifference_Op);

      // Convert the region to a list of rectangles.
      for (SkRegion::Iterator i(region); !i.done(); i.next()) {
        opaque_region.push_back(gfx::SkIRectToRect(i.rect()));
      }
    } else {
      // The entire window except for the translucent top is opaque.
      gfx::Rect opaque_region_dip(widget_size);
      gfx::Insets insets;
      insets.set_top(view->GetTranslucentTopAreaHeight());
      opaque_region_dip.Inset(insets);
      opaque_region.push_back(
          gfx::ScaleToEnclosingRect(opaque_region_dip, scale));
    }
    window->SetOpaqueRegion(opaque_region);
  }

  SizeConstraintsChanged();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostLinux,
//     DesktopWindowTreeHostLinuxImpl implementation:

void BrowserDesktopWindowTreeHostLinux::Init(
    const views::Widget::InitParams& params) {
  DesktopWindowTreeHostLinux::Init(std::move(params));

#if defined(USE_DBUS_MENU)
  // We have now created our backing X11 window.  We now need to (possibly)
  // alert the desktop environment that there's a menu bar attached to it.
  if (CreateGlobalMenuBar()) {
    dbus_appmenu_ =
        std::make_unique<DbusAppmenu>(browser_view_, GetAcceleratedWidget());
  }
#endif
}

void BrowserDesktopWindowTreeHostLinux::OnWidgetInitDone() {
  DesktopWindowTreeHostLinux::OnWidgetInitDone();

  UpdateFrameHints();
}

void BrowserDesktopWindowTreeHostLinux::CloseNow() {
#if defined(USE_DBUS_MENU)
  dbus_appmenu_.reset();
#endif
  DesktopWindowTreeHostLinux::CloseNow();
}

void BrowserDesktopWindowTreeHostLinux::Show(
    ui::mojom::WindowShowState show_state,
    const gfx::Rect& restore_bounds) {
  DesktopWindowTreeHostLinux::Show(show_state, restore_bounds);

  const std::string& startup_id =
      browser_view_->browser()->create_params().startup_id;
  if (!startup_id.empty() && !SentStartupIds().count(startup_id)) {
    platform_window()->NotifyStartupComplete(startup_id);
    SentStartupIds().insert(startup_id);
  }
}

bool BrowserDesktopWindowTreeHostLinux::IsOverrideRedirect() const {
  auto* x11_extension = GetX11Extension();
  return (browser_frame_->tab_drag_kind() == TabDragKind::kAllTabs) &&
         x11_extension && x11_extension->IsWmTiling() &&
         x11_extension->CanResetOverrideRedirect();
}

gfx::Insets BrowserDesktopWindowTreeHostLinux::CalculateInsetsInDIP(
    ui::PlatformWindowState window_state) const {
  // If we are not showing frame, the insets should be zero.
  if (!IsShowingFrame(browser_frame_->native_browser_frame()->UseCustomFrame(),
                      window_state)) {
    return gfx::Insets();
  }

  return static_cast<BrowserNonClientFrameView*>(browser_frame_->GetFrameView())
      ->RestoredMirroredFrameBorderInsets();
}

void BrowserDesktopWindowTreeHostLinux::OnWindowStateChanged(
    ui::PlatformWindowState old_state,
    ui::PlatformWindowState new_state) {
  DesktopWindowTreeHostLinux::OnWindowStateChanged(old_state, new_state);

  bool fullscreen_changed = ui::IsPlatformWindowStateFullscreen(new_state) ||
                            ui::IsPlatformWindowStateFullscreen(old_state);
  if (old_state != new_state && fullscreen_changed) {
    // If the browser view initiated this state change,
    // BrowserView::ProcessFullscreen will no-op, so this call is harmless.
    browser_view_->FullscreenStateChanging();

    // In sync fullscreen window state mode, the FullscreenStateChanged()
    // is called just after requesting to set fullscreen property
    // to the platform, so not to call here to avoid duplicated invocation.
    if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
      browser_view_->FullscreenStateChanged();
    }
  }

  UpdateFrameHints();
}

void BrowserDesktopWindowTreeHostLinux::OnWindowTiledStateChanged(
    ui::WindowTiledEdges new_tiled_edges) {
  bool maximized = new_tiled_edges.top && new_tiled_edges.left &&
                   new_tiled_edges.bottom && new_tiled_edges.right;
  bool tiled = new_tiled_edges.top || new_tiled_edges.left ||
               new_tiled_edges.bottom || new_tiled_edges.right;
  browser_frame_->set_tiled(tiled && !maximized);
  UpdateFrameHints();
  if (SupportsClientFrameShadow()) {
    // Trigger a re-layout as the insets will change even if the bounds don't.
    ScheduleRelayout();
    if (GetWidget()->non_client_view()) {
      // This is needed for the decorated regions, borders etc. to be repainted.
      GetWidget()->non_client_view()->SchedulePaint();
    }
  }
}

void BrowserDesktopWindowTreeHostLinux::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  UpdateFrameHints();
}

void BrowserDesktopWindowTreeHostLinux::OnDeviceScaleFactorChanged() {
  UpdateFrameHints();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostLinux(native_widget_delegate,
                                               desktop_native_widget_aura,
                                               browser_view, browser_frame);
}
