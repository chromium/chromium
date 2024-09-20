// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/ash/login/oobe_ui_dialog_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/ash/login/login_display_host_mojo.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view.h"
#include "ui/views/view_shadow.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace ash {
namespace {

constexpr char kGaiaURL[] = "chrome://oobe/gaia-signin";

}  // namespace

// TODO(b/314987456): This is a duplicate of the same class used for cleanup in
// InlineLoginDialog and a few others. They can be consolidated.
//
// Cleans up the delegate for a WebContentsModalDialogManager on destruction, or
// on WebContents destruction, whichever comes first.
class OobeUIDialogDelegate::ModalDialogManagerCleanup
    : public content::WebContentsObserver {
 public:
  // This constructor automatically observes |web_contents| for its lifetime.
  explicit ModalDialogManagerCleanup(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ModalDialogManagerCleanup(const ModalDialogManagerCleanup&) = delete;
  ModalDialogManagerCleanup& operator=(const ModalDialogManagerCleanup&) =
      delete;
  ~ModalDialogManagerCleanup() override { ResetDelegate(); }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { ResetDelegate(); }

  void ResetDelegate() {
    if (!web_contents()) {
      return;
    }
    web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
        ->SetDelegate(nullptr);
  }
};

class OobeWebDialogView : public views::WebDialogView {
  METADATA_HEADER(OobeWebDialogView, views::WebDialogView)

 public:
  OobeWebDialogView(content::BrowserContext* context,
                    ui::WebDialogDelegate* delegate,
                    std::unique_ptr<WebContentsHandler> handler)
      : views::WebDialogView(context, delegate, std::move(handler)) {
    if (features::IsOobeJellyEnabled() || features::IsBootAnimationEnabled()) {
      set_use_round_corners(/*round=*/true);
      set_corner_radius(kOobeDialogCornerRadius);
    }
  }

  OobeWebDialogView(const OobeWebDialogView&) = delete;
  OobeWebDialogView& operator=(const OobeWebDialogView&) = delete;

  // content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    // This is required for accessing the camera for SAML logins.
    MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
        web_contents, request, std::move(callback), nullptr /* extension */);
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
  }

  bool TakeFocus(content::WebContents* source, bool reverse) override {
    LoginScreen::Get()->FocusLoginShelf(reverse);
    return true;
  }

  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

  OobeUI* GetOobeUI() {
    content::WebUI* webui = web_contents()->GetWebUI();
    if (webui) {
      return static_cast<OobeUI*>(webui->GetController());
    }
    return nullptr;
  }

  // Overridden from views::View:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    // Return the focus to the web contents.
    web_contents()->FocusThroughTabTraversal(reverse);
    GetWidget()->Activate();
    web_contents()->Focus();
  }

 private:
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

BEGIN_METADATA(OobeWebDialogView)
END_METADATA

// View that controls size of OobeUIDialog.
// Dialog can be shown as a full-screen (in this case it will fit whole screen)
// except for Virtual Keyboard or as a window.
// For both dimensions we try to center dialog so that it has size no more than
// kGaiaDialogMaxSize with margins no less than kGaiaDialogMinMargin on each
// side.
// If available height is too small (usually due to virtual keyboard),
// and calculations above would give total dialog height lesser than
// kGaiaDialogMinHeight, then margins will be reduced to accommodate minimum
// height.
// When no virtual keyboard is displayed, ash shelf height should be excluded
// from space available for calculations.
// Host size accounts for virtual keyboard presence, but not for shelf.
// It is assumed that host view is always a full-screen view on a primary
// display.
class LayoutWidgetDelegateView : public views::WidgetDelegateView {
  METADATA_HEADER(LayoutWidgetDelegateView, views::WidgetDelegateView)

 public:
  LayoutWidgetDelegateView(OobeUIDialogDelegate* dialog_delegate,
                           OobeWebDialogView* oobe_view)
      : dialog_delegate_(dialog_delegate), oobe_view_(oobe_view) {
    SetFocusTraversesOut(true);
    AddChildView(oobe_view_.get());

    if (features::IsOobeJellyEnabled() || features::IsBootAnimationEnabled()) {
      // Create a shadow for the OOBE dialog.
      view_shadow_ = std::make_unique<views::ViewShadow>(
          oobe_view_.get(), kOobeDialogShadowElevation);
      view_shadow_->SetRoundedCornerRadius(kOobeDialogCornerRadius);
    }
  }

  LayoutWidgetDelegateView(const LayoutWidgetDelegateView&) = delete;
  LayoutWidgetDelegateView& operator=(const LayoutWidgetDelegateView&) = delete;

  ~LayoutWidgetDelegateView() override { delete dialog_delegate_; }

  void SetFullscreen(bool value) {
    if (fullscreen_ == value) {
      return;
    }
    fullscreen_ = value;
    OnPropertyChanged(&fullscreen_, views::kPropertyEffectsLayout);
  }
  bool GetFullscreen() const { return fullscreen_; }

  void SetHasShelf(bool value) {
    if (has_shelf_ == value) {
      return;
    }
    has_shelf_ = value;
    OnPropertyChanged(&has_shelf_, views::kPropertyEffectsLayout);
  }
  bool GetHasShelf() const { return has_shelf_; }

  // views::WidgetDelegateView:
  ui::mojom::ModalType GetModalType() const override {
    return ui::mojom::ModalType::kWindow;
  }

  void Layout(PassKey) override {
    if (fullscreen_) {
      oobe_view_->SetBoundsRect(GetContentsBounds());
      return;
    }

    gfx::Rect bounds;
    const int shelf_height = has_shelf_ ? ShelfConfig::Get()->shelf_size() : 0;
    const gfx::Size display_size =
        display::Screen::GetScreen()->GetPrimaryDisplay().size();
    const bool is_horizontal = display_size.width() > display_size.height();
    CalculateOobeDialogBounds(GetContentsBounds(), shelf_height, is_horizontal,
                              &bounds);
    oobe_view_->SetBoundsRect(bounds);
  }

  View* GetInitiallyFocusedView() override { return oobe_view_; }

 private:
  raw_ptr<OobeUIDialogDelegate, DanglingUntriaged> dialog_delegate_ =
      nullptr;                                      // Owned by us.
  raw_ptr<OobeWebDialogView> oobe_view_ = nullptr;  // Owned by views hierarchy.
  std::unique_ptr<views::ViewShadow> view_shadow_;

  // Indicates whether Oobe web view should fully occupy the hosting widget.
  bool fullscreen_ = false;
  // Indicates if ash shelf is displayed (and should be excluded from available
  // space).
  bool has_shelf_ = true;
};

BEGIN_METADATA(LayoutWidgetDelegateView)
ADD_PROPERTY_METADATA(bool, Fullscreen)
ADD_PROPERTY_METADATA(bool, HasShelf)
END_METADATA

OobeUIDialogDelegate::OobeUIDialogDelegate(
    base::WeakPtr<LoginDisplayHostMojo> controller)
    : controller_(controller) {
  set_allow_default_context_menu(false);
  set_can_close(true);
  set_can_resize(false);
  set_dialog_content_url(GURL(kGaiaURL));
  set_dialog_modal_type(ui::mojom::ModalType::kWindow);
  set_show_dialog_title(false);
  keyboard_observer_.Observe(ChromeKeyboardControllerClient::Get());

  for (size_t i = 0; i < kLoginAcceleratorDataLength; ++i) {
    if (kLoginAcceleratorData[i].global) {
      continue;
    }
    if (!(kLoginAcceleratorData[i].scope & (kScopeLogin | kScopeLock))) {
      continue;
    }

    accel_map_[ui::Accelerator(kLoginAcceleratorData[i].keycode,
                               kLoginAcceleratorData[i].modifiers)] =
        kLoginAcceleratorData[i].action;
  }

  DCHECK(!dialog_view_ && !widget_);
  // Life cycle of `dialog_view_` is managed by the widget:
  // Widget owns a root view which has `dialog_view_` as its child view.
  // Before the widget is destroyed, it will clean up the view hierarchy
  // starting from root view.
  dialog_view_ =
      new OobeWebDialogView(ProfileHelper::GetSigninProfile(), this,
                            std::make_unique<ChromeWebContentsHandler>());
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  ash_util::SetupWidgetInitParamsForContainerInPrimary(
      &params, kShellWindowId_LockScreenContainer);
  layout_view_ = new LayoutWidgetDelegateView(this, dialog_view_);
  params.delegate = layout_view_.get();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.show_state = ui::mojom::WindowShowState::kFullscreen;

  widget_ = new views::Widget();
  widget_->Init(std::move(params));

  layout_view_->SetHasShelf(
      !ChromeKeyboardControllerClient::Get()->is_keyboard_visible());

  view_observer_.Observe(dialog_view_.get());
  GetOobeUI()->GetErrorScreen()->MaybeInitCaptivePortalWindowProxy(
      GetWebContents());
  oobe_ui_observer_.Observe(GetOobeUI());
  captive_portal_observer_.Observe(
      GetOobeUI()->GetErrorScreen()->captive_portal_window_proxy());
  // Set this as the web modal delegate so that web dialog can appear. E.g.
  // for the proxy auth.
  auto* web_contents = GetWebContents();
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
  modal_dialog_manager_cleanup_ =
      std::make_unique<ModalDialogManagerCleanup>(web_contents);
}

OobeUIDialogDelegate::~OobeUIDialogDelegate() {
  view_observer_.Reset();
  // Reset scoped observation of the captive portal before closing the captive
  // portal delegate as it posts the task which can trigger
  // `OnAfterCaptivePortalHidden` to be called after `OobeUIDialogDelegate`
  // destruction.
  captive_portal_observer_.Reset();
  if (controller_) {
    controller_->OnDialogDestroyed(this);
  }
  for (auto& observer : modal_dialog_host_observer_list_) {
    observer.OnHostDestroying();
  }
}

content::WebContents* OobeUIDialogDelegate::GetWebContents() {
  return dialog_view_->web_contents();
}

bool OobeUIDialogDelegate::IsVisible() {
  return widget_->IsVisible();
}

void OobeUIDialogDelegate::SetShouldDisplayCaptivePortal(bool should_display) {
  should_display_captive_portal_ = should_display;
}

void OobeUIDialogDelegate::Show() {
  if (auto* client = LoginScreenClientImpl::Get()) {
    scoped_system_tray_observer_.Reset();
    scoped_system_tray_observer_.Observe(client);
  }
  widget_->Show();
  if (state_ == OobeDialogState::HIDDEN) {
    SetState(OobeDialogState::GAIA_SIGNIN);
  } else {
    LoginScreen::Get()->GetModel()->NotifyOobeDialogState(state_);
  }

  if (should_display_captive_portal_) {
    GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
  }
}

void OobeUIDialogDelegate::ShowFullScreen() {
  layout_view_->SetFullscreen(true);
  Show();
}

void OobeUIDialogDelegate::Hide() {
  scoped_system_tray_observer_.Reset();
  if (!widget_) {
    return;
  }
  widget_->Hide();
  SetState(OobeDialogState::HIDDEN);
}

void OobeUIDialogDelegate::Close() {
  if (!widget_) {
    return;
  }
  SetState(OobeDialogState::HIDDEN);

  // We do not call LoginScreen::NotifyOobeDialogVisibility here, because this
  // would cause the LoginShelfView to update its button visibility even though
  // the login screen is about to be destroyed. See http://crbug/836172
  widget_->Close();
}

void OobeUIDialogDelegate::SetState(OobeDialogState state) {
  if (!widget_ || state_ == state) {
    return;
  }

  state_ = state;

  // Gaia WebUI is preloaded, so it's possible for WebUI to send state updates
  // while the widget is not visible. Defer the state update until Show().
  if (!widget_->IsVisible() && state_ != OobeDialogState::HIDDEN) {
    return;
  }

  LoginScreen::Get()->GetModel()->NotifyOobeDialogState(state_);
}

OobeUI* OobeUIDialogDelegate::GetOobeUI() const {
  if (dialog_view_) {
    return dialog_view_->GetOobeUI();
  }
  return nullptr;
}

gfx::NativeWindow OobeUIDialogDelegate::GetNativeWindow() const {
  return widget_ ? widget_->GetNativeWindow() : nullptr;
}

views::Widget* OobeUIDialogDelegate::GetWebDialogWidget() const {
  return widget_;
}

views::View* OobeUIDialogDelegate::GetWebDialogView() {
  return dialog_view_;
}

void OobeUIDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  widget_->Close();
}

std::vector<ui::Accelerator> OobeUIDialogDelegate::GetAccelerators() {
  // TODO(crbug.com/40561667): Adding necessary accelerators.
  std::vector<ui::Accelerator> output;

  for (const auto& pair : accel_map_) {
    output.push_back(pair.first);
  }

  return output;
}

bool OobeUIDialogDelegate::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  auto entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end()) {
    return false;
  }
  if (controller_) {
    return controller_->HandleAccelerator(entry->second);
  }
  return false;
}

void OobeUIDialogDelegate::OnViewBoundsChanged(views::View* observed_view) {
  if (!widget_) {
    return;
  }
  GetOobeUI()->GetCoreOobe()->UpdateClientAreaSize(
      layout_view_->GetContentsBounds().size());
}

void OobeUIDialogDelegate::OnViewIsDeleting(views::View* observed_view) {
  DCHECK_EQ(observed_view, dialog_view_);
  view_observer_.Reset();
  dialog_view_ = nullptr;
}

void OobeUIDialogDelegate::OnKeyboardVisibilityChanged(bool visible) {
  if (!widget_) {
    return;
  }
  layout_view_->SetHasShelf(!visible);
}

void OobeUIDialogDelegate::OnBeforeCaptivePortalShown() {
  should_display_captive_portal_ = false;
}

void OobeUIDialogDelegate::OnAfterCaptivePortalHidden() {
  // If the captive portal state went from hidden -> visible -> back to hidden
  // while the OOBE dialog was not shown, we should not attempt to load the
  // captive portal next time the OOBE dialog pops up.
  should_display_captive_portal_ = false;
}

void OobeUIDialogDelegate::OnCurrentScreenChanged(OobeScreenId current_screen,
                                                  OobeScreenId new_screen) {}

void OobeUIDialogDelegate::OnDestroyingOobeUI() {
  captive_portal_observer_.Reset();
  oobe_ui_observer_.Reset();
}

void OobeUIDialogDelegate::OnFocusLeavingSystemTray(bool reverse) {
  if (dialog_view_) {
    dialog_view_->AboutToRequestFocusFromTabTraversal(reverse);
  }
}

web_modal::WebContentsModalDialogHost*
OobeUIDialogDelegate::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView OobeUIDialogDelegate::GetHostView() const {
  return widget_->GetNativeView();
}

gfx::Point OobeUIDialogDelegate::GetDialogPosition(const gfx::Size& size) {
  // Center the widget.
  gfx::Size widget_size = widget_->GetWindowBoundsInScreen().size();
  return gfx::Point(widget_size.width() / 2 - size.width() / 2,
                    widget_size.height() / 2 - size.height() / 2);
}

gfx::Size OobeUIDialogDelegate::GetMaximumDialogSize() {
  return widget_->GetWindowBoundsInScreen().size();
}

void OobeUIDialogDelegate::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  if (observer && !modal_dialog_host_observer_list_.HasObserver(observer)) {
    modal_dialog_host_observer_list_.AddObserver(observer);
  }
}

void OobeUIDialogDelegate::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  modal_dialog_host_observer_list_.RemoveObserver(observer);
}

ui::WebDialogDelegate::FrameKind OobeUIDialogDelegate::GetWebDialogFrameKind()
    const {
  return (features::IsOobeJellyEnabled() || features::IsBootAnimationEnabled())
             ? ui::WebDialogDelegate::FrameKind::kDialog
             : ui::WebDialogDelegate::FrameKind::kNonClient;
}

}  // namespace ash
