// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/document_overlay_window_views.h"

#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_image_button.h"
#include "chrome/browser/ui/views/overlay/back_to_tab_label_button.h"
#include "chrome/browser/ui/views/overlay/close_image_button.h"
#include "chrome/browser/ui/views/overlay/resize_handle_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/resize_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/vector_icons.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/rounded_corner_utils.h"
#include "ash/public/cpp/window_properties.h"  // nogncheck
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/win/shell.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/aura/window_tree_host.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

namespace {

// The opacity of the controls scrim.
constexpr double kControlsScrimOpacity = 0.76;

#if BUILDFLAG(IS_CHROMEOS_ASH)
// The opacity of the resize handle control.
constexpr double kResizeHandleOpacity = 0.38;
#endif

// Size of a secondary control.
constexpr gfx::Size kSecondaryControlSize(20, 20);

// Margin from the bottom of the window for secondary controls.
constexpr int kSecondaryControlBottomMargin = 16;

template <typename T>
T* AddChildView(std::vector<std::unique_ptr<views::View>>* views,
                std::unique_ptr<T> child) {
  views->push_back(std::move(child));
  return static_cast<T*>(views->back().get());
}

}  // namespace

// static
std::unique_ptr<DocumentOverlayWindowViews> DocumentOverlayWindowViews::Create(
    content::DocumentPictureInPictureWindowController* controller) {
  DVLOG(1) << __func__ << ": DocumentOverlayWindowViews::Create";
  // Can't use make_unique(), which doesn't have access to the private
  // constructor. It's important that the constructor be private, because it
  // doesn't initialize the object fully.
  auto overlay_window =
      base::WrapUnique(new DocumentOverlayWindowViews(controller));

  overlay_window->CalculateAndUpdateWindowBounds();
  overlay_window->SetUpViews();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Just to have any non-empty bounds as required by Init(). The window is
  // resized to fit the WebView that is embedded right afterwards, anyway.
  params.bounds = gfx::Rect(overlay_window->GetMinimumSize());
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  params.name = "PictureInPictureWindow";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.delegate = OverlayWindowViews::CreateDelegate();

  overlay_window->Init(std::move(params));
  overlay_window->OnRootViewReady();

#if BUILDFLAG(IS_WIN)
  std::wstring app_user_model_id;
  Browser* browser =
      chrome::FindBrowserWithWebContents(controller->GetWebContents());
  if (browser) {
    const base::FilePath& profile_path = browser->profile()->GetPath();
    // Set the window app id to GetAppUserModelIdForApp if the original window
    // is an app window, GetAppUserModelIdForBrowser if it's a browser window.
    app_user_model_id =
        browser->is_type_app()
            ? shell_integration::win::GetAppUserModelIdForApp(
                  base::UTF8ToWide(browser->app_name()), profile_path)
            : shell_integration::win::GetAppUserModelIdForBrowser(profile_path);
    if (!app_user_model_id.empty()) {
      ui::win::SetAppIdForWindow(
          app_user_model_id,
          overlay_window->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  return overlay_window;
}

// static
std::unique_ptr<content::DocumentOverlayWindow>
content::DocumentOverlayWindow::Create(
    content::DocumentPictureInPictureWindowController* controller) {
  return DocumentOverlayWindowViews::Create(controller);
}

DocumentOverlayWindowViews::DocumentOverlayWindowViews(
    content::DocumentPictureInPictureWindowController* controller)
    : controller_(controller) {}

DocumentOverlayWindowViews::~DocumentOverlayWindowViews() = default;

bool DocumentOverlayWindowViews::ControlsHitTestContainsPoint(
    const gfx::Point& point) {
  if (!AreControlsVisible())
    return false;
  if (GetBackToTabControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point)) {
    return true;
  }
  return false;
}

content::PictureInPictureWindowController*
DocumentOverlayWindowViews::GetController() const {
  return controller_;
}

views::View* DocumentOverlayWindowViews::GetWindowBackgroundView() const {
  return window_background_view_;
}

views::View* DocumentOverlayWindowViews::GetControlsContainerView() const {
  return controls_container_view_;
}

void DocumentOverlayWindowViews::SetUpViews() {
  auto web_view = std::make_unique<views::WebView>();
  DVLOG(2) << __func__ << ": content WebView=" << web_view.get();
  content::WebContents* pip_contents = controller_->GetChildWebContents();
  web_view->SetWebContents(pip_contents);

  // views::View that is displayed when WebView is hidden. ---------------------
  // Adding an extra pixel to width/height makes sure controls background cover
  // entirely window when platform has fractional scale applied.
  auto window_background_view = std::make_unique<views::View>();
  auto controls_scrim_view = std::make_unique<views::View>();
  auto controls_container_view = std::make_unique<views::View>();
  auto close_controls_view =
      std::make_unique<CloseImageButton>(base::BindRepeating(
          [](DocumentOverlayWindowViews* overlay) {
            const bool should_pause = true;
            overlay->controller_->Close(should_pause);
          },
          base::Unretained(this)));

  std::unique_ptr<BackToTabImageButton> back_to_tab_image_button;
  std::unique_ptr<BackToTabLabelButton> back_to_tab_label_button;
  auto back_to_tab_callback = base::BindRepeating(
      [](DocumentOverlayWindowViews* overlay) {
        overlay->controller_->CloseAndFocusInitiator();
      },
      base::Unretained(this));
  if (base::FeatureList::IsEnabled(media::kMediaSessionWebRTC)) {
    back_to_tab_label_button =
        std::make_unique<BackToTabLabelButton>(std::move(back_to_tab_callback));
  } else {
    back_to_tab_image_button =
        std::make_unique<BackToTabImageButton>(std::move(back_to_tab_callback));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto resize_handle_view =
      std::make_unique<ResizeHandleButton>(views::Button::PressedCallback());
#endif

  window_background_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  window_background_view->layer()->SetName("WindowBackgroundView");
  window_background_view->layer()->SetColor(SK_ColorBLACK);

  // view::View that holds the WebView. ---------------------------------------
  web_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  web_view->layer()->SetMasksToBounds(true);
  web_view->layer()->SetFillsBoundsOpaquely(false);
  web_view->layer()->SetName("WebView");

  // views::View that holds the scrim, which appears with the controls. -------
  controls_scrim_view->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  controls_scrim_view->layer()->SetName("ControlsScrimView");
  controls_scrim_view->layer()->SetColor(gfx::kGoogleGrey900);
  controls_scrim_view->layer()->SetOpacity(kControlsScrimOpacity);

  // views::View that is a parent of all the controls. Makes hiding and showing
  // all the controls at once easier.
  controls_container_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  controls_container_view->layer()->SetFillsBoundsOpaquely(false);
  controls_container_view->layer()->SetName("ControlsContainerView");

  // views::View that closes the window. --------------------------------------
  close_controls_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  close_controls_view->layer()->SetFillsBoundsOpaquely(false);
  close_controls_view->layer()->SetName("CloseControlsView");

  // views::View that closes the window and focuses initiator tab. ------------
  if (back_to_tab_image_button) {
    back_to_tab_image_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    back_to_tab_image_button->layer()->SetFillsBoundsOpaquely(false);
    back_to_tab_image_button->layer()->SetName("BackToTabControlsView");
  } else {
    DCHECK(back_to_tab_label_button);
    back_to_tab_label_button->SetPaintToLayer(ui::LAYER_TEXTURED);
    back_to_tab_label_button->layer()->SetFillsBoundsOpaquely(false);
    back_to_tab_label_button->layer()->SetName("BackToTabControlsView");
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // views::View that shows the affordance that the window can be resized. ----
  resize_handle_view->SetPaintToLayer(ui::LAYER_TEXTURED);
  resize_handle_view->layer()->SetFillsBoundsOpaquely(false);
  resize_handle_view->layer()->SetName("ResizeHandleView");
  resize_handle_view->layer()->SetOpacity(kResizeHandleOpacity);
#endif

  // Set up view::Views hierarchy. --------------------------------------------
  window_background_view_ =
      AddChildView(&view_holder_, std::move(window_background_view));
  web_view_ = AddChildView(&view_holder_, std::move(web_view));
  controls_scrim_view_ =
      controls_container_view->AddChildView(std::move(controls_scrim_view));
  close_controls_view_ =
      controls_container_view->AddChildView(std::move(close_controls_view));

  if (back_to_tab_image_button) {
    back_to_tab_image_button_ = controls_container_view->AddChildView(
        std::move(back_to_tab_image_button));
  } else {
    DCHECK(back_to_tab_label_button);
    back_to_tab_label_button_ = controls_container_view->AddChildView(
        std::move(back_to_tab_label_button));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  resize_handle_view_ =
      controls_container_view->AddChildView(std::move(resize_handle_view));
#endif
  controls_container_view_ =
      AddChildView(&view_holder_, std::move(controls_container_view));
}

void DocumentOverlayWindowViews::OnRootViewReady() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetNativeWindow()->SetProperty(ash::kWindowPipTypeKey, true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  GetRootView()->SetPaintToLayer(ui::LAYER_TEXTURED);
  GetRootView()->layer()->SetName("RootView");
  GetRootView()->layer()->SetMasksToBounds(true);

  views::View* const contents_view = GetContentsView();
  for (std::unique_ptr<views::View>& child : view_holder_)
    contents_view->AddChildView(std::move(child));
  view_holder_.clear();

  // Don't show the controls until the mouse hovers over the window.
  UpdateControlsVisibility(false);

  // FIXME, get aspect/size via PiP API
  UpdateNaturalSize({400, 300});
}

void DocumentOverlayWindowViews::UpdateLayerBoundsWithLetterboxing(
    gfx::Size window_size) {
  // This is the case when the window is initially created or the video surface
  // id has not been embedded.
  if (!native_widget() || GetBounds().IsEmpty() || GetNaturalSize().IsEmpty())
    return;

  gfx::Rect letterbox_region = media::ComputeLetterboxRegion(
      gfx::Rect(gfx::Point(0, 0), window_size), GetNaturalSize());
  if (letterbox_region.IsEmpty())
    return;

  // To avoid black stripes in the window when integer window dimensions don't
  // correspond to the content aspect ratio exactly (e.g. 854x480 for 16:9
  // video) force the letterbox region size to be equal to the window size.
  const float aspect_ratio =
      static_cast<float>(GetNaturalSize().width()) / GetNaturalSize().height();
  if (aspect_ratio > 1 && window_size.height() == letterbox_region.height()) {
    const int height_from_width =
        base::ClampRound(window_size.width() / aspect_ratio);
    if (height_from_width == window_size.height())
      letterbox_region.set_width(window_size.width());
  } else if (aspect_ratio <= 1 &&
             window_size.width() == letterbox_region.width()) {
    const int width_from_height =
        base::ClampRound(window_size.height() * aspect_ratio);
    if (width_from_height == window_size.width())
      letterbox_region.set_height(window_size.height());
  }

  const gfx::Rect content_bounds(
      gfx::Point((window_size.width() - letterbox_region.size().width()) / 2,
                 (window_size.height() - letterbox_region.size().height()) / 2),
      letterbox_region.size());

  // Update the layout of the controls.
  UpdateControlsBounds();

  // Update the surface layer bounds to scale with window size changes.
  window_background_view_->SetBoundsRect(
      gfx::Rect(gfx::Point(0, 0), GetBounds().size()));
  web_view_->SetBoundsRect(content_bounds);
  if (web_view_->layer()->has_external_content())
    web_view_->layer()->SetSurfaceSize(content_bounds.size());
}

void DocumentOverlayWindowViews::OnUpdateControlsBounds() {
  controls_container_view_->SetSize(GetBounds().size());

  // Adding an extra pixel to width/height makes sure the scrim covers the
  // entire window when the platform has fractional scaling applied.
  gfx::Rect larger_window_bounds = gfx::Rect(GetBounds().size());
  larger_window_bounds.Inset(-1, -1);
  controls_scrim_view_->SetBoundsRect(larger_window_bounds);

  OverlayWindowViews::WindowQuadrant quadrant =
      OverlayWindowViews::GetCurrentWindowQuadrant(GetBounds(), controller_);
  close_controls_view_->SetPosition(GetBounds().size(), quadrant);

  if (back_to_tab_label_button_)
    back_to_tab_label_button_->SetWindowSize(GetBounds().size());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  UpdateResizeHandleBounds(quadrant);
#endif

  // Following controls order matters:
  // #1 Back to tab
  std::vector<views::ImageButton*> visible_controls_views;
  if (back_to_tab_image_button_)
    visible_controls_views.push_back(back_to_tab_image_button_);

  if (visible_controls_views.size() > 4)
    visible_controls_views.resize(4);

  int mid_window_x = GetBounds().size().width() / 2;
  int secondary_control_y = GetBounds().size().height() -
                            kSecondaryControlSize.height() -
                            kSecondaryControlBottomMargin;

  switch (visible_controls_views.size()) {
    case 0:
      DCHECK(back_to_tab_label_button_);
      break;
    case 1: {
      /* | --- --- [ ] --- --- | */
      visible_controls_views[0]->SetSize(kSecondaryControlSize);
      visible_controls_views[0]->SetPosition(
          gfx::Point(mid_window_x - kSecondaryControlSize.width() / 2,
                     secondary_control_y));
      break;
    }
    default:
      NOTREACHED();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void DocumentOverlayWindowViews::UpdateResizeHandleBounds(
    OverlayWindowViews::WindowQuadrant quadrant) {
  resize_handle_view_->SetPosition(GetBounds().size(), quadrant);
  GetNativeWindow()->SetProperty(
      ash::kWindowPipResizeHandleBoundsKey,
      new gfx::Rect(GetResizeHandleControlsBounds()));
}
#endif

bool DocumentOverlayWindowViews::IsActive() {
  return views::Widget::IsActive();
}

bool DocumentOverlayWindowViews::IsActive() const {
  return views::Widget::IsActive();
}

void DocumentOverlayWindowViews::Close() {
  views::Widget::Close();
}

void DocumentOverlayWindowViews::ShowInactive() {
  DoShowInactive();
}

void DocumentOverlayWindowViews::Hide() {
  views::Widget::Hide();
}

bool DocumentOverlayWindowViews::IsVisible() {
  return views::Widget::IsVisible();
}

bool DocumentOverlayWindowViews::IsVisible() const {
  return views::Widget::IsVisible();
}

bool DocumentOverlayWindowViews::IsAlwaysOnTop() {
  return true;
}

gfx::Rect DocumentOverlayWindowViews::GetBounds() {
  return views::Widget::GetRestoredBounds();
}

void DocumentOverlayWindowViews::UpdateNaturalSize(
    const gfx::Size& natural_size) {
  DoUpdateNaturalSize(natural_size);
}

void DocumentOverlayWindowViews::OnNativeWidgetMove() {
  OverlayWindowViews::OnNativeWidgetMove();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Update the positioning of some icons when the window is moved.
  WindowQuadrant quadrant =
      GetCurrentWindowQuadrant(GetRestoredBounds(), GetController());
  close_controls_view_->SetPosition(GetRestoredBounds().size(), quadrant);
  UpdateResizeHandleBounds(quadrant);
#endif
}

void DocumentOverlayWindowViews::OnNativeWidgetDestroyed() {
  views::Widget::OnNativeWidgetDestroyed();
  controller_->OnWindowDestroyed(
      /*should_pause_video=*/true);
}

void DocumentOverlayWindowViews::OnGestureEvent(ui::GestureEvent* event) {
  if (OverlayWindowViews::OnGestureEventHandledOrIgnored(event))
    return;

  if (GetBackToTabControlsBounds().Contains(event->location())) {
    controller_->CloseAndFocusInitiator();
    event->SetHandled();
  } else if (GetCloseControlsBounds().Contains(event->location())) {
    controller_->Close(/*should_pause_video=*/true);
    event->SetHandled();
  }
}

gfx::Rect DocumentOverlayWindowViews::GetBackToTabControlsBounds() {
  if (back_to_tab_image_button_)
    return back_to_tab_image_button_->GetMirroredBounds();

  DCHECK(back_to_tab_label_button_);
  return back_to_tab_label_button_->GetMirroredBounds();
}

gfx::Rect DocumentOverlayWindowViews::GetCloseControlsBounds() {
  return close_controls_view_->GetMirroredBounds();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
gfx::Rect DocumentOverlayWindowViews::GetResizeHandleControlsBounds() {
  return resize_handle_view_->GetMirroredBounds();
}

int DocumentOverlayWindowViews::GetResizeHTComponent() const {
  return resize_handle_view_->GetHTComponent();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

CloseImageButton* DocumentOverlayWindowViews::close_button_for_testing() const {
  return close_controls_view_;
}

ui::Layer* DocumentOverlayWindowViews::document_layer_for_testing() const {
  return web_view_->layer();
}
