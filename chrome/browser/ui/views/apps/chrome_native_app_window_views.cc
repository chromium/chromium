// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

#include <stddef.h>

#include <utility>

#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "extensions/browser/extension_util.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindow;

namespace {

const AcceleratorMapping kAppWindowAcceleratorMap[] = {
  { ui::VKEY_W, ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW },
};

// These accelerators will only be available in kiosk mode. These allow the
// user to manually zoom app windows. This is only necessary in kiosk mode
// (in normal mode, the user can zoom via the screen magnifier).
// TODO(xiyuan): Write a test for kiosk accelerators.
const AcceleratorMapping kAppWindowKioskAppModeAcceleratorMap[] = {
    {ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS},
    {ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_ZOOM_MINUS},
    {ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS},
    {ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS},
    {ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS},
    {ui::VKEY_ADD, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS},
    {ui::VKEY_0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL},
    {ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL},
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_DEV_TOOLS},
    {ui::VKEY_J, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_DEV_TOOLS_CONSOLE},
    {ui::VKEY_C, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
     IDC_DEV_TOOLS_INSPECT}};

std::map<ui::Accelerator, int> AcceleratorsFromMapping(
    const AcceleratorMapping mapping_array[],
    size_t mapping_length) {
  std::map<ui::Accelerator, int> mapping;
  for (size_t i = 0; i < mapping_length; ++i) {
    ui::Accelerator accelerator(mapping_array[i].keycode,
                                mapping_array[i].modifiers);
    mapping.insert(std::make_pair(accelerator, mapping_array[i].command_id));
  }

  return mapping;
}

const std::map<ui::Accelerator, int>& GetAcceleratorTable() {
  if (!IsRunningInForcedAppMode()) {
    static base::NoDestructor<std::map<ui::Accelerator, int>> accelerators(
        AcceleratorsFromMapping(kAppWindowAcceleratorMap,
                                std::size(kAppWindowAcceleratorMap)));
    return *accelerators;
  }

  static base::NoDestructor<std::map<ui::Accelerator, int>>
      app_mode_accelerators([]() {
        std::map<ui::Accelerator, int> mapping = AcceleratorsFromMapping(
            kAppWindowAcceleratorMap, std::size(kAppWindowAcceleratorMap));
        std::map<ui::Accelerator, int> kiosk_mapping = AcceleratorsFromMapping(
            kAppWindowKioskAppModeAcceleratorMap,
            std::size(kAppWindowKioskAppModeAcceleratorMap));
        mapping.insert(std::begin(kiosk_mapping), std::end(kiosk_mapping));
        return mapping;
      }());
  return *app_mode_accelerators;
}

}  // namespace

ChromeNativeAppWindowViews::ChromeNativeAppWindowViews() = default;

ChromeNativeAppWindowViews::~ChromeNativeAppWindowViews() = default;

void ChromeNativeAppWindowViews::OnBeforeWidgetInit(
    const AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
}

void ChromeNativeAppWindowViews::InitializeDefaultWindow(
    const AppWindow::CreateParams& create_params) {
  views::Widget::InitParams init_params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  init_params.delegate = this;
  init_params.remove_standard_frame = ShouldRemoveStandardFrame();
  if (create_params.alpha_enabled) {
    init_params.opacity =
        views::Widget::InitParams::WindowOpacity::kTranslucent;

    // The given window is most likely not rectangular since it uses
    // transparency and has no standard frame, don't show a shadow for it.
    // TODO(skuhne): If we run into an application which should have a shadow
    // but does not have, a new attribute has to be added.
    if (IsFrameless())
      init_params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  }
  if (create_params.always_on_top)
    init_params.z_order = ui::ZOrderLevel::kFloatingWindow;
  init_params.visible_on_all_workspaces =
      create_params.visible_on_all_workspaces;

  OnBeforeWidgetInit(create_params, &init_params, widget());
  gfx::Rect init_param_bounds = init_params.bounds;
  widget()->Init(std::move(init_params));

  // The frame insets and window radii are required to resolve the bounds
  // specifications correctly. So we set the window bounds and constraints now.
  gfx::Insets frame_insets = GetFrameInsets();
  gfx::Rect window_bounds =
      init_param_bounds.IsEmpty()
          ? create_params.GetInitialWindowBounds(frame_insets, GetWindowRadii())
          : init_param_bounds;
  SetContentSizeConstraints(create_params.GetContentMinimumSize(frame_insets),
                            create_params.GetContentMaximumSize(frame_insets));
  if (!window_bounds.IsEmpty()) {
    using BoundsSpecification = AppWindow::BoundsSpecification;
    bool position_specified =
        window_bounds.x() != BoundsSpecification::kUnspecifiedPosition &&
        window_bounds.y() != BoundsSpecification::kUnspecifiedPosition;
    if (!position_specified) {
#if BUILDFLAG(IS_MAC)
      // On Mac, this will call NativeWidgetMac's CenterWindow() which relies
      // on the size being its content size instead of window size. That
      // API only causes a problem when we use system title bar in an old
      // platform app.
      gfx::Rect content_bounds = window_bounds;
      content_bounds.Inset(frame_insets);
      widget()->CenterWindow(content_bounds.size());
#else
      widget()->CenterWindow(window_bounds.size());
#endif
    } else {
      widget()->SetBounds(window_bounds);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (create_params.is_ime_window)
    return;
#endif

  // Register accelarators supported by app windows.
  views::FocusManager* focus_manager = GetFocusManager();
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  const bool is_kiosk_app_mode = IsRunningInForcedAppMode();

  // Ensures that kiosk mode accelerators are enabled when in kiosk mode (to be
  // future proof). This is needed because GetAcceleratorTable() uses a static
  // to store data and only checks kiosk mode once. If a platform app is
  // launched before kiosk mode starts, the kiosk accelerators will not be
  // registered. This CHECK catches the case.
  CHECK(!is_kiosk_app_mode ||
        accelerator_table.size() ==
            std::size(kAppWindowAcceleratorMap) +
                std::size(kAppWindowKioskAppModeAcceleratorMap));

  // Ensure there is a ZoomController in kiosk mode, otherwise the processing
  // of the accelerators will cause a crash. Note CHECK here because DCHECK
  // will not be noticed, as this could only be relevant on real hardware.
  CHECK(!is_kiosk_app_mode ||
        zoom::ZoomController::FromWebContents(web_view()->GetWebContents()));

  for (auto iter = accelerator_table.begin(); iter != accelerator_table.end();
       ++iter) {
    if (is_kiosk_app_mode &&
        !IsCommandAllowedInAppMode(iter->second, /* is_popup */ false)) {
      continue;
    }

    focus_manager->RegisterAccelerator(
        iter->first, ui::AcceleratorManager::kNormalPriority, this);
  }
}

std::unique_ptr<views::NonClientFrameView>
ChromeNativeAppWindowViews::CreateStandardDesktopAppFrame() {
  return views::WidgetDelegateView::CreateNonClientFrameView(widget());
}

bool ChromeNativeAppWindowViews::ShouldRemoveStandardFrame() {
  return IsFrameless() || has_frame_color_;
}

// ui::BaseWindow implementation.

gfx::Rect ChromeNativeAppWindowViews::GetRestoredBounds() const {
  return widget()->GetRestoredBounds();
}

ui::mojom::WindowShowState ChromeNativeAppWindowViews::GetRestoredState()
    const {
  if (IsMaximized())
    return ui::mojom::WindowShowState::kMaximized;
  if (IsFullscreen())
    return ui::mojom::WindowShowState::kFullscreen;

  return ui::mojom::WindowShowState::kNormal;
}

ui::ZOrderLevel ChromeNativeAppWindowViews::GetZOrderLevel() const {
  return widget()->GetZOrderLevel();
}

// views::WidgetDelegate implementation.

ui::ImageModel ChromeNativeAppWindowViews::GetWindowAppIcon() {
  // Resulting icon is cached in aura::client::kAppIconKey window property.
  const gfx::Image& custom_image = GetCustomImage();
  if (app_window()->app_icon_url().is_valid() &&
      app_window()->show_in_shelf()) {
    EnsureAppIconCreated();
    gfx::Image base_image =
        !custom_image.IsEmpty()
            ? custom_image
            : gfx::Image(extensions::util::GetDefaultAppIcon());
    // Scale the icon to EXTENSION_ICON_LARGE.
    const int large_icon_size = extension_misc::EXTENSION_ICON_LARGE;
    if (base_image.Width() != large_icon_size ||
        base_image.Height() != large_icon_size) {
      gfx::ImageSkia resized_image =
          gfx::ImageSkiaOperations::CreateResizedImage(
              base_image.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
              gfx::Size(large_icon_size, large_icon_size));
      return ui::ImageModel::FromImageSkia(
          gfx::ImageSkiaOperations::CreateIconWithBadge(
              resized_image, GetAppIconImage().AsImageSkia()));
    }
    return ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateIconWithBadge(
            base_image.AsImageSkia(), GetAppIconImage().AsImageSkia()));
  }

  if (!custom_image.IsEmpty())
    return ui::ImageModel::FromImage(custom_image);
  EnsureAppIconCreated();
  return ui::ImageModel::FromImage(GetAppIconImage());
}

ui::ImageModel ChromeNativeAppWindowViews::GetWindowIcon() {
  // Resulting icon is cached in aura::client::kWindowIconKey window property.
  content::WebContents* web_contents = app_window()->web_contents();
  if (web_contents) {
    favicon::FaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(web_contents);
    return ui::ImageModel::FromImage(favicon_driver->GetFavicon());
  }
  return ui::ImageModel();
}

std::unique_ptr<views::NonClientFrameView>
ChromeNativeAppWindowViews::CreateNonClientFrameView(views::Widget* widget) {
  return (IsFrameless() || has_frame_color_) ?
      CreateNonStandardAppFrame() : CreateStandardDesktopAppFrame();
}

bool ChromeNativeAppWindowViews::WidgetHasHitTestMask() const {
  return shape_ != nullptr;
}

void ChromeNativeAppWindowViews::GetWidgetHitTestMask(SkPath* mask) const {
  shape_->getBoundaryPath(mask);
}

// views::View implementation.

bool ChromeNativeAppWindowViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const std::map<ui::Accelerator, int>& accelerator_table =
      GetAcceleratorTable();
  auto iter = accelerator_table.find(accelerator);
  CHECK(iter != accelerator_table.end(), base::NotFatalUntil::M130);
  int command_id = iter->second;
  switch (command_id) {
    case IDC_CLOSE_WINDOW:
      Close();
      return true;
    case IDC_ZOOM_MINUS:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                           content::PAGE_ZOOM_OUT);
      return true;
    case IDC_ZOOM_NORMAL:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                           content::PAGE_ZOOM_RESET);
      return true;
    case IDC_ZOOM_PLUS:
      zoom::PageZoom::Zoom(web_view()->GetWebContents(), content::PAGE_ZOOM_IN);
      return true;
    case IDC_DEV_TOOLS:
      DevToolsWindow::OpenDevToolsWindow(
          web_view()->GetWebContents(), DevToolsToggleAction::Show(),
          DevToolsOpenedByAction::kMainMenuOrMainShortcut);
      return true;
    case IDC_DEV_TOOLS_CONSOLE:
      DevToolsWindow::OpenDevToolsWindow(
          web_view()->GetWebContents(),
          DevToolsToggleAction::ShowConsolePanel(),
          DevToolsOpenedByAction::kConsoleShortcut);
      return true;
    case IDC_DEV_TOOLS_INSPECT:
      DevToolsWindow::OpenDevToolsWindow(
          web_view()->GetWebContents(), DevToolsToggleAction::Inspect(),
          DevToolsOpenedByAction::kInspectorModeShortcut);
      return true;
    default:
      NOTREACHED() << "Unknown accelerator sent to app window.";
  }
}

// NativeAppWindow implementation.

void ChromeNativeAppWindowViews::SetFullscreen(int fullscreen_types) {
  widget()->SetFullscreen(fullscreen_types != AppWindow::FULLSCREEN_TYPE_NONE);
}

bool ChromeNativeAppWindowViews::IsFullscreenOrPending() const {
  return widget()->IsFullscreen();
}

void ChromeNativeAppWindowViews::UpdateShape(
    std::unique_ptr<ShapeRects> rects) {
  shape_rects_ = std::move(rects);

  // Build a region from the list of rects when it is supplied.
  std::unique_ptr<SkRegion> region;
  if (shape_rects_) {
    region = std::make_unique<SkRegion>();
    for (const gfx::Rect& input_rect : *shape_rects_)
      region->op(gfx::RectToSkIRect(input_rect), SkRegion::kUnion_Op);
  }
  shape_ = std::move(region);
  OnWidgetHasHitTestMaskChanged();
  widget()->SetShape(shape() ? std::make_unique<ShapeRects>(*shape_rects_)
                             : nullptr);
  widget()->OnSizeConstraintsChanged();
}

bool ChromeNativeAppWindowViews::HasFrameColor() const {
  return has_frame_color_;
}

SkColor ChromeNativeAppWindowViews::ActiveFrameColor() const {
  return active_frame_color_;
}

SkColor ChromeNativeAppWindowViews::InactiveFrameColor() const {
  return inactive_frame_color_;
}

// NativeAppWindowViews implementation.

void ChromeNativeAppWindowViews::InitializeWindow(
    AppWindow* app_window,
    const AppWindow::CreateParams& create_params) {
  DCHECK(widget());
  has_frame_color_ = create_params.has_frame_color;
  active_frame_color_ = create_params.active_frame_color;
  inactive_frame_color_ = create_params.inactive_frame_color;
  InitializeDefaultWindow(create_params);
  extension_keybinding_registry_ =
      std::make_unique<ExtensionKeybindingRegistryViews>(
          Profile::FromBrowserContext(app_window->browser_context()),
          widget()->GetFocusManager(),
          extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY, nullptr);
}

gfx::Image ChromeNativeAppWindowViews::GetCustomImage() {
  return app_window()->custom_app_icon();
}

gfx::Image ChromeNativeAppWindowViews::GetAppIconImage() {
  DCHECK(app_icon_);
  return gfx::Image(app_icon_->image_skia());
}

void ChromeNativeAppWindowViews::EnsureAppIconCreated() {
  if (app_icon_ && app_icon_->IsValid())
    return;

  // To avoid recursive call, reset the smart pointer. It will be checked in
  // OnIconUpdated to determine if this is a real update or the initial callback
  // on icon creation.
  app_icon_.reset();
  app_icon_ =
      extensions::ChromeAppIconService::Get(app_window()->browser_context())
          ->CreateIcon(this, app_window()->extension_id(),
                       app_window()->app_delegate()->PreferredIconSize());
}

void ChromeNativeAppWindowViews::OnIconUpdated(
    extensions::ChromeAppIcon* icon) {
  if (!app_icon_)
    return;
  DCHECK_EQ(app_icon_.get(), icon);
  UpdateWindowIcon();
}
