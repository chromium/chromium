// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"

#include "base/json/json_writer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/layout/layout_provider.h"
#include "url/gurl.h"

// TODO(b/223434114): Add tests for AccessCodeCastDialog

namespace media_router {

namespace {

void SetCurrentDialog(std::unique_ptr<AccessCodeCastDialog> dialog) {
  static base::NoDestructor<std::unique_ptr<AccessCodeCastDialog>> instance;
  DCHECK(!dialog || !*instance)
      << "Can't show AccessCodeCastDialog when it is alreasdy being shown!";
  *instance = std::move(dialog);
}

}  // namespace

// The corner radius for system dialogs.
constexpr int kSystemDialogCornerRadiusDp = 12;

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
}

AccessCodeCastDialog::~AccessCodeCastDialog() {
  if (dialog_widget_)
    dialog_widget_->RemoveObserver(this);
}

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
      absl::make_optional<views::Widget::InitParams>(std::move(extra_params)));

  auto* dialog_widget = views::Widget::GetWidgetForNativeWindow(dialog_window);
  ObserveWidget(dialog_widget);

  if (dialog_mode == AccessCodeCastDialogMode::kBrowserStandard &&
      web_contents_) {
    constrained_window::UpdateWidgetModalDialogPosition(
        dialog_widget,
        CreateChromeConstrainedWindowViewsClient()->GetModalDialogHost(
            web_contents_->GetTopLevelNativeWindow()));
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
  AccessCodeCastMetrics::RecordDialogOpenLocation(open_location);
  SetCurrentDialog(std::move(dialog));
}

// static
void AccessCodeCastDialog::ShowForDesktopMirroring(
    AccessCodeCastDialogOpenLocation open_location) {
  CastModeSet desktop_mode = {MediaCastMode::DESKTOP_MIRROR};
  std::unique_ptr<MediaRouteStarter> starter =
      std::make_unique<MediaRouteStarter>(desktop_mode, nullptr, nullptr);
  Show(desktop_mode, std::move(starter), open_location,
       AccessCodeCastDialogMode::kSystem);
}

views::Widget::InitParams AccessCodeCastDialog::CreateParams(
    AccessCodeCastDialogMode dialog_mode) {
  views::Widget::InitParams params;
  params.remove_standard_frame = true;
  // Use the corner radius which matches style based on the appropriate mode.
  params.corner_radius =
      (dialog_mode == AccessCodeCastDialogMode::kBrowserStandard)
          ? views::LayoutProvider::Get()->GetCornerRadiusMetric(
                views::Emphasis::kMedium)
          : kSystemDialogCornerRadiusDp;
  params.type = views::Widget::InitParams::Type::TYPE_BUBBLE;
  // Make sure the dialog border is rendered correctly
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  return params;
}

// views::WidgetObserver:
void AccessCodeCastDialog::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
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

ui::ModalType AccessCodeCastDialog::GetDialogModalType() const {
  // If there are no web_contents_, that means that the dialog was launched
  // from the system tray, so therefore it shuold be a system dialog.
  return web_contents_ ? ui::MODAL_TYPE_NONE : ui::MODAL_TYPE_SYSTEM;
}

std::u16string AccessCodeCastDialog::GetDialogTitle() const {
  return std::u16string();
}

GURL AccessCodeCastDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIAccessCodeCastURL);
}

void AccessCodeCastDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void AccessCodeCastDialog::GetDialogSize(gfx::Size* size) const {
  const int kDefaultWidth = 448;
  const int kDefaultHeight = 271;
  const int kRememberDevicesHeight = 310;
  base::TimeDelta duration_pref = GetAccessCodeDeviceDurationPref(
      context_->GetPrefs());
  bool rememberDevices = duration_pref != base::Seconds(0);
  size->SetSize(kDefaultWidth,
      rememberDevices ? kRememberDevicesHeight : kDefaultHeight);
}

std::string AccessCodeCastDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

void AccessCodeCastDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
  AccessCodeCastUI* controller =
      webui_->GetController()->GetAs<AccessCodeCastUI>();
  controller->SetCastModeSet(cast_mode_set_);
  controller->SetDialogCreationTimestamp(dialog_creation_timestamp_);
  controller->SetMediaRouteStarter(std::move(media_route_starter_));
}

void AccessCodeCastDialog::OnDialogClosed(const std::string& json_retval) {
  // Setting the global ptr to null will cause the existing dialog (this) to be
  // destructed.
  SetCurrentDialog(nullptr);
}

void AccessCodeCastDialog::OnCloseContents(content::WebContents* source,
                                           bool* out_close_dialog) {
  *out_close_dialog = true;
  closing_dialog_ = true;
}

bool AccessCodeCastDialog::ShouldShowDialogTitle() const {
  return false;
}

bool AccessCodeCastDialog::ShouldShowCloseButton() const {
  return false;
}

AccessCodeCastDialog::FrameKind AccessCodeCastDialog::GetWebDialogFrameKind()
    const {
  return FrameKind::kDialog;
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
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return true;
}

gfx::NativeView AccessCodeCastDialog::GetParentView() {
  gfx::NativeView parent = nullptr;

  if (web_contents_) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        web_contents_->GetTopLevelNativeWindow());
    DCHECK(widget) << "Could not find a parent widget!";
    if (widget)
      parent = widget->GetNativeView();
  }

  return parent;
}

void AccessCodeCastDialog::ObserveWidget(views::Widget* widget) {
  DCHECK(widget) << "Observed dialog widget must not be null";
  dialog_widget_ = widget;
  dialog_widget_->AddObserver(this);
}

}  // namespace media_router
