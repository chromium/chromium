// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/ash/login/webui_login_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::content::RenderViewHost;
using ::content::WebContents;
using ::input::NativeWebKeyboardEvent;
using ::web_modal::WebContentsModalDialogManager;

// A class to change arrow key traversal behavior when it's alive.
class ScopedArrowKeyTraversal {
 public:
  explicit ScopedArrowKeyTraversal(bool new_arrow_key_tranversal_enabled)
      : previous_arrow_key_traversal_enabled_(
            views::FocusManager::arrow_key_traversal_enabled()) {
    views::FocusManager::set_arrow_key_traversal_enabled(
        new_arrow_key_tranversal_enabled);
  }

  ScopedArrowKeyTraversal(const ScopedArrowKeyTraversal&) = delete;
  ScopedArrowKeyTraversal& operator=(const ScopedArrowKeyTraversal&) = delete;

  ~ScopedArrowKeyTraversal() {
    views::FocusManager::set_arrow_key_traversal_enabled(
        previous_arrow_key_traversal_enabled_);
  }

 private:
  const bool previous_arrow_key_traversal_enabled_;
};

void InitializeWebView(views::WebView* web_view) {
  WebContents* web_contents = web_view->GetWebContents();

  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents, SK_ColorTRANSPARENT);

  // Ensure that the login UI has a tab ID, which will allow the GAIA auth
  // extension's background script to tell it apart from a captive portal window
  // that may be opened on top of this UI.
  CreateSessionServiceTabHelper(web_contents);

  // Create the password manager that is needed for the proxy.
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents);
  ChromePasswordManagerClient::CreateForWebContents(web_contents);

  // Create the password reuse detection manager.
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);

  // LoginHandlerViews uses a constrained window for the password manager view.
  WebContentsModalDialogManager::CreateForWebContents(web_contents);

  extensions::SetViewType(web_contents,
                          extensions::mojom::ViewType::kComponent);
  blink::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
      prefs, ProfileHelper::GetSigninProfile());
}

}  // namespace

// WebUILoginView public: ------------------------------------------------------

WebUILoginView::WebUILoginView(base::WeakPtr<LoginDisplayHostWebUI> controller)
    : controller_(controller) {
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &WebUILoginView::OnAppTerminating, base::Unretained(this)));

  session_observation_.Observe(session_manager::SessionManager::Get());

  for (size_t i = 0; i < kLoginAcceleratorDataLength; ++i) {
    ui::Accelerator accelerator(kLoginAcceleratorData[i].keycode,
                                kLoginAcceleratorData[i].modifiers);
    accel_map_[accelerator] = kLoginAcceleratorData[i].action;
  }

  for (AccelMap::iterator i(accel_map_.begin()); i != accel_map_.end(); ++i) {
    AddAccelerator(i->first);
  }

  if (LoginScreenClientImpl::HasInstance()) {
    LoginScreenClientImpl::Get()->AddSystemTrayObserver(this);
    observing_system_tray_focus_ = true;
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kWindow);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_OOBE_ACCESSIBLE_SCREEN_NAME));
}

WebUILoginView::~WebUILoginView() {
  for (auto& observer : observer_list_) {
    observer.OnHostDestroying();
  }

  // TODO(crbug.com/1188526) - Improve the observation of the system tray
  if (observing_system_tray_focus_ && LoginScreenClientImpl::HasInstance()) {
    LoginScreenClientImpl::Get()->RemoveSystemTrayObserver(this);
  }

  // Clear any delegates we have set on the WebView.
  WebContents* web_contents = web_view_->GetWebContents();
  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(nullptr);
  web_contents->SetDelegate(nullptr);
}

void WebUILoginView::Init() {
  // Init() should only be called once.
  DCHECK(!web_view_);
  auto web_view =
      std::make_unique<views::WebView>(ProfileHelper::GetSigninProfile());
  WebContents* web_contents = web_view->GetWebContents();

  InitializeWebView(web_view.get());
  web_view->set_allow_accelerators(true);

  web_view_ = AddChildView(std::move(web_view));

  WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
  web_contents->SetDelegate(this);
}

void WebUILoginView::RequestFocus() {
  web_view_->RequestFocus();
}

web_modal::WebContentsModalDialogHost*
WebUILoginView::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView WebUILoginView::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point WebUILoginView::GetDialogPosition(const gfx::Size& size) {
  // Center the widget.
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(widget_size.width() / 2 - size.width() / 2,
                    widget_size.height() / 2 - size.height() / 2);
}

gfx::Size WebUILoginView::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void WebUILoginView::AddObserver(web_modal::ModalDialogHostObserver* observer) {
  if (observer && !observer_list_.HasObserver(observer)) {
    observer_list_.AddObserver(observer);
  }
}

void WebUILoginView::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool WebUILoginView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  AccelMap::const_iterator entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end()) {
    return false;
  }
  if (controller_) {
    return controller_->HandleAccelerator(entry->second);
  }
  return false;
}

gfx::NativeWindow WebUILoginView::GetNativeWindow() const {
  return GetWidget()->GetNativeWindow();
}

void WebUILoginView::LoadURL(const GURL& url) {
  web_view_->LoadInitialURL(url);
  web_view_->RequestFocus();
}

content::WebUI* WebUILoginView::GetWebUI() {
  return web_view_->web_contents()->GetWebUI();
}

content::WebContents* WebUILoginView::GetWebContents() {
  return web_view_->web_contents();
}

OobeUI* WebUILoginView::GetOobeUI() {
  if (!GetWebUI()) {
    return nullptr;
  }

  return static_cast<OobeUI*>(GetWebUI()->GetController());
}

void WebUILoginView::OnPostponedShow() {
  set_is_hidden(false);
  OnLoginPromptVisible();
}

void WebUILoginView::SetKeyboardEventsAndSystemTrayEnabled(bool enabled) {
  forward_keyboard_event_ = enabled;

  SystemTrayClientImpl::Get()->SetPrimaryTrayEnabled(enabled);
}

// WebUILoginView protected: ---------------------------------------------------

void WebUILoginView::Layout(PassKey) {
  DCHECK(web_view_);
  web_view_->SetBoundsRect(bounds());

  for (auto& observer : observer_list_) {
    observer.OnPositionRequiresUpdate();
  }
}

void WebUILoginView::ChildPreferredSizeChanged(View* child) {
  DeprecatedLayoutImmediately();
  SchedulePaint();
}

void WebUILoginView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // Return the focus to the web contents.
  web_view_->web_contents()->FocusThroughTabTraversal(reverse);
  GetWidget()->Activate();
  web_view_->web_contents()->Focus();
}

void WebUILoginView::OnAppTerminating() {
  // In some tests, WebUILoginView remains after LoginScreenClientImpl gets
  // deleted on shutdown. It should unregister itself before the deletion
  // happens.
  if (observing_system_tray_focus_) {
    LoginScreenClientImpl::Get()->RemoveSystemTrayObserver(this);
    observing_system_tray_focus_ = false;
  }
}

void WebUILoginView::OnLoginOrLockScreenVisible() {
  OnLoginPromptVisible();
  session_observation_.Reset();
}

// WebUILoginView private: -----------------------------------------------------

bool WebUILoginView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
#ifndef NDEBUG
  // Do not show the context menu.
  return false;
#else
  return true;
#endif
}

bool WebUILoginView::HandleKeyboardEvent(content::WebContents* source,
                                         const NativeWebKeyboardEvent& event) {
  bool handled = false;
  if (forward_keyboard_event_) {
    // Disable arrow key traversal because arrow keys are handled via
    // accelerator when this view has focus.
    ScopedArrowKeyTraversal arrow_key_traversal(false);

    handled = unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

  return handled;
}

bool WebUILoginView::TakeFocus(content::WebContents* source, bool reverse) {
  // In case of blocked UI (ex.: sign in is in progress)
  // we should not process focus change events.
  if (!forward_keyboard_event_) {
    return false;
  }

  // FocusLoginShelf focuses either system tray or login shelf buttons.
  // Only do this if the login shelf is enabled.
  if (shelf_enabled_) {
    LoginScreen::Get()->FocusLoginShelf(reverse);
  }
  return shelf_enabled_;
}

void WebUILoginView::RequestMediaAccessPermission(
    WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Note: This is needed for taking photos when selecting new user images
  // and SAML logins. Must work for all user types (including supervised).
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool WebUILoginView::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

bool WebUILoginView::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

void WebUILoginView::OnFocusLeavingSystemTray(bool reverse) {
  AboutToRequestFocusFromTabTraversal(reverse);
}

void WebUILoginView::OnSystemTrayBubbleShown() {}

void WebUILoginView::OnLoginPromptVisible() {
  if (!observing_system_tray_focus_ && LoginScreenClientImpl::HasInstance()) {
    LoginScreenClientImpl::Get()->AddSystemTrayObserver(this);
    observing_system_tray_focus_ = true;
  }
  // If we're hidden than will generate this signal once we're shown.
  if (is_hidden_ || webui_visible_) {
    VLOG(1) << "Login WebUI >> not emitting signal, hidden: " << is_hidden_;
    return;
  }
  TRACE_EVENT0("chromeos", "WebUILoginView::OnLoginPromptVisible");
  if (should_emit_login_prompt_visible_) {
    VLOG(1) << "Login WebUI >> login-prompt-visible";
    SessionManagerClient::Get()->EmitLoginPromptVisible();
  }

  webui_visible_ = true;
}

BEGIN_METADATA(WebUILoginView)
END_METADATA

}  // namespace ash
