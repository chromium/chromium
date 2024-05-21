// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"

#include "base/json/json_writer.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/layout/layout_provider.h"
#include "url/gurl.h"

using web_modal::ModalDialogHost;

namespace media_router {

namespace {

void SetCurrentDialog(base::WeakPtr<AccessCodeCastDialog> dialog) {
  // Keeps track of the dialog that is currently being displayed.
  static base::NoDestructor<base::WeakPtr<AccessCodeCastDialog>>
      current_instance;
  if (*current_instance)
    // Closing the dialog will cause the dialog to delete itself.
    (*current_instance)->CloseDialogWidget();
  if (dialog)
    *current_instance = std::move(dialog);
}

void UpdateDialogPosition(views::Widget* widget,
                          content::WebContents* web_contents) {
  auto* dialog_host =
      CreateChromeConstrainedWindowViewsClient()->GetModalDialogHost(
          web_contents->GetTopLevelNativeWindow());
  views::Widget* host_widget =
      views::Widget::GetWidgetForNativeView(dialog_host->GetHostView());

  // If the host view is not backed by a Views::Widget, just update the widget
  // size.
  auto size = widget->GetRootView()->GetPreferredSize();
  if (!host_widget) {
    widget->SetSize(size);
    return;
  }

  // Get the outer browser window for the web_contents.
  auto* browser_window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents);
  auto window_bounds = browser_window->GetBounds();

  gfx::Point position = dialog_host->GetDialogPosition(size);
  // Align the first row of pixels inside the border. This is the apparent top
  // of the dialog.
  position.set_y(position.y() -
                 widget->non_client_view()->frame_view()->GetInsets().top());

  if (widget->is_top_level()) {
    position += host_widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();
    // Move the dialog to the center of the browser window.
    auto new_x = window_bounds.x() + (window_bounds.width() - size.width()) / 2;
    position.set_x(new_x);
    // If the dialog extends partially off any display, clamp its position to
    // be fully visible within that display. If the dialog doesn't intersect
    // with any display clamp its position to be fully on the nearest display.
    gfx::Rect display_rect = gfx::Rect(position, size);
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestView(
            dialog_host->GetHostView());
    const gfx::Rect work_area = display.work_area();

    if (!work_area.Contains(display_rect))
      display_rect.AdjustToFit(work_area);
    position = display_rect.origin();
  }

  widget->SetBounds(gfx::Rect(position, size));
}

}  // namespace

// The corner radius for system dialogs.
constexpr int kSystemDialogCornerRadiusDp = 12;

// The default width, height without footnote, height with footnote for the
// dialog container.
constexpr gfx::Size kDialogSizeWithoutFootnote{448, 295};
constexpr gfx::Size kDialogSizeWithFootnote{448, 330};

// static
bool AccessCodeCastDialog::block_widget_activation_changed_for_test_ = false;

AccessCodeCastDialog::AccessCodeCastDialog(
    const CastModeSet& cast_mode_set,
    std::unique_ptr<MediaRouteStarter> media_route_starter)
    : cast_mode_set_(cast_mode_set),
      media_route_starter_(std::move(media_route_starter)),
      web_contents_(media_route_starter_->GetWebContents()),
      context_(media_route_starter_->GetProfile()) {
  DCHECK(media_route_starter_) << "Must have a media route starter!";
  DCHECK(!cast_mode_set_.empty())
      << "Must have at least one available casting mode!";
  DCHECK(*cast_mode_set_.begin() == MediaCastMode::DESKTOP_MIRROR ||
         web_contents_)
      << "Web contents must be set for non desktop-mode casting!";
  set_can_resize(false);
  set_dialog_args("{}");
  set_dialog_content_url(GURL(chrome::kChromeUIAccessCodeCastURL));
  set_dialog_frame_kind(FrameKind::kDialog);
  set_show_close_button(false);
  set_show_dialog_title(false);

  base::TimeDelta duration = GetAccessCodeDeviceDurationPref(context_);
  const bool remember_devices = duration != base::Seconds(0);
  set_dialog_size(remember_devices ? kDialogSizeWithFootnote
                                   : kDialogSizeWithoutFootnote);
}

AccessCodeCastDialog::~AccessCodeCastDialog() = default;

void AccessCodeCastDialog::ShowWebDialog(AccessCodeCastDialogMode dialog_mode) {
  // After a dialog is shown, |media_route_starter_| is transferred to the
  // associated |AccessCodeCastUI| - see |OnDialogShown| below. Since the c'tor
  // ensures that a |MediaRouteStarter| is passed in, if |media_route_starter_|
  // is nullptr, it means that |ShowWebDialog| was already called.
  DCHECK(media_route_starter_) << "Cannot show dialog more than once!";
  if (!media_route_starter_)
    return;

  auto extra_params = CreateParams(dialog_mode);

  dialog_creation_timestamp_ = base::Time::Now();
  gfx::NativeWindow dialog_window = chrome::ShowWebDialogWithParams(
      GetParentView(), context_, this,
      std::make_optional<views::Widget::InitParams>(std::move(extra_params)));

  dialog_widget_ = views::Widget::GetWidgetForNativeWindow(dialog_window);
  widget_observation_.Observe(dialog_widget_.get());

  if (dialog_mode == AccessCodeCastDialogMode::kBrowserStandard &&
      web_contents_) {
    UpdateDialogPosition(dialog_widget_, web_contents_);
  }
}

// static
void AccessCodeCastDialog::Show(
    const media_router::CastModeSet& cast_mode_set,
    std::unique_ptr<media_router::MediaRouteStarter> media_route_starter,
    AccessCodeCastDialogOpenLocation open_location,
    AccessCodeCastDialogMode dialog_mode) {
  std::unique_ptr<AccessCodeCastDialog> dialog =
      std::make_unique<AccessCodeCastDialog>(cast_mode_set,
                                             std::move(media_route_starter));
  dialog->ShowWebDialog(dialog_mode);
  // Release the pointer from the unique_ptr after ShowWebDialog() since now the
  // lifetime of the dialog is being managed by the WebDialog Delegate. The
  // dialog will delete itself when OnDialogClosed() is called.
  base::WeakPtr<AccessCodeCastDialog> new_dialog =
      dialog.release()->GetWeakPtr();
  AccessCodeCastMetrics::RecordDialogOpenLocation(open_location);
  SetCurrentDialog(std::move(new_dialog));
}

// static
void AccessCodeCastDialog::ShowForDesktopMirroring(
    AccessCodeCastDialogOpenLocation open_location) {
  CastModeSet desktop_mode = {MediaCastMode::DESKTOP_MIRROR};
  std::unique_ptr<MediaRouteStarter> starter =
      std::make_unique<MediaRouteStarter>(
          MediaRouterUIParameters(desktop_mode, nullptr));
  Show(desktop_mode, std::move(starter), open_location,
       AccessCodeCastDialogMode::kSystem);
}

views::Widget::InitParams AccessCodeCastDialog::CreateParams(
    AccessCodeCastDialogMode dialog_mode) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.remove_standard_frame = true;
  // If we are acting as a system dialog, use the appropriate corner radius.
  // Otherwise, the widget will default to the correct value for browser
  // dialogs.
  if (dialog_mode == AccessCodeCastDialogMode::kSystem) {
    params.corner_radius = kSystemDialogCornerRadiusDp;
  }
  params.type = views::Widget::InitParams::Type::TYPE_BUBBLE;
  // Make sure the dialog border is rendered correctly
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  return params;
}

void AccessCodeCastDialog::CloseDialogWidget() {
  dialog_widget_->Close();
}

base::WeakPtr<AccessCodeCastDialog> AccessCodeCastDialog::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// views::WidgetObserver:
void AccessCodeCastDialog::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  if (block_widget_activation_changed_for_test_)
    return;
  DCHECK(dialog_widget_)
      << "dialog_widget_ must be set exactly once during dialog setup";
  // Close the dialog only if it is no longer active and it isn't already
  // closing.
  if (dialog_widget_ && !active && !closing_dialog_) {
    AccessCodeCastMetrics::RecordDialogCloseReason(
        AccessCodeCastDialogCloseReason::kFocus);
    dialog_widget_->Close();
  }
}

void AccessCodeCastDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
  AccessCodeCastUI* controller =
      webui_->GetController()->GetAs<AccessCodeCastUI>();
  controller->SetCastModeSet(cast_mode_set_);
  controller->SetDialogCreationTimestamp(dialog_creation_timestamp_);
  controller->SetMediaRouteStarter(std::move(media_route_starter_));
}

void AccessCodeCastDialog::OnCloseContents(content::WebContents* source,
                                           bool* out_close_dialog) {
  *out_close_dialog = true;
  closing_dialog_ = true;
}

// Ensure the WebUI dialog has camera access
void AccessCodeCastDialog::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool AccessCodeCastDialog::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return true;
}

gfx::NativeView AccessCodeCastDialog::GetParentView() {
  gfx::NativeView parent = gfx::NativeView();

  if (web_contents_) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        web_contents_->GetTopLevelNativeWindow());
    DCHECK(widget) << "Could not find a parent widget!";
    if (widget)
      parent = widget->GetNativeView();
  }

  return parent;
}

}  // namespace media_router
