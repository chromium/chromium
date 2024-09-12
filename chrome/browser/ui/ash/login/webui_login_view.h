// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_WEBUI_LOGIN_VIEW_H_

#include <map>
#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "ash/system/tray/system_tray_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace content {
class WebUI;
}

namespace views {
class View;
class WebView;
class Widget;
}  // namespace views

namespace ash {
class LoginDisplayHostWebUI;
class OobeUI;

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up. It contains a WebView.
class WebUILoginView : public views::View,
                       public content::WebContentsDelegate,
                       public session_manager::SessionManagerObserver,
                       public ChromeWebModalDialogManagerDelegate,
                       public web_modal::WebContentsModalDialogHost,
                       public SystemTrayObserver {
  METADATA_HEADER(WebUILoginView, views::View)

 public:
  explicit WebUILoginView(base::WeakPtr<LoginDisplayHostWebUI> controller);

  WebUILoginView(const WebUILoginView&) = delete;
  WebUILoginView& operator=(const WebUILoginView&) = delete;

  ~WebUILoginView() override;

  // Initializes the webui login view.
  virtual void Init();

  // Overridden from views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void RequestFocus() override;

  // Overridden from ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Overridden from web_modal::WebContentsModalDialogHost:
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  // Gets the native window from the view widget.
  gfx::NativeWindow GetNativeWindow() const;

  // Loads given page. Should be called after Init() has been called.
  void LoadURL(const GURL& url);

  // Returns current WebUI.
  content::WebUI* GetWebUI();

  // Returns current WebContents.
  content::WebContents* GetWebContents();

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI();

  // Called when WebUI is being shown after being initilized hidden.
  void OnPostponedShow();

  // Sets whether keyboard events can be forwarded from the WebUI and the system
  // tray is available.
  void SetKeyboardEventsAndSystemTrayEnabled(bool enabled);

  void set_is_hidden(bool hidden) { is_hidden_ = hidden; }

  // Let suppress emission of this signal.
  void set_should_emit_login_prompt_visible(bool emit) {
    should_emit_login_prompt_visible_ = emit;
  }

  void set_shelf_enabled(bool enabled) { shelf_enabled_ = enabled; }

 protected:
  // Overridden from views::View:
  void Layout(PassKey) override;
  void ChildPreferredSizeChanged(View* child) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // session_manager::SessionManagerObserver:
  void OnLoginOrLockScreenVisible() override;

 private:
  void OnAppTerminating();

  // Map type for the accelerator-to-identifier map.
  typedef std::map<ui::Accelerator, LoginAcceleratorAction> AccelMap;

  // Overridden from content::WebContentsDelegate.
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;

  // Overridden from SystemTrayObserver.
  void OnFocusLeavingSystemTray(bool reverse) override;
  void OnSystemTrayBubbleShown() override;

  // Performs series of actions when login prompt is considered
  // to be ready and visible.
  // 1. Emits LoginPromptVisible signal if needed
  // 2. Notifies OOBE/sign classes.
  void OnLoginPromptVisible();

  base::CallbackListSubscription on_app_terminating_subscription_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  base::WeakPtr<LoginDisplayHostWebUI> controller_;

  // WebView for rendering a webpage as a webui login.
  raw_ptr<views::WebView> web_view_ = nullptr;

  // Converts keyboard events on the WebContents to accelerators.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Maps installed accelerators to OOBE webui accelerator identifiers.
  AccelMap accel_map_;

  // True when WebUI is being initialized hidden.
  bool is_hidden_ = false;

  // True when the WebUI has finished initializing and is visible.
  bool webui_visible_ = false;

  // Should we emit the login-prompt-visible signal when the login page is
  // displayed?
  bool should_emit_login_prompt_visible_ = true;

  // True to forward keyboard event.
  bool forward_keyboard_event_ = true;

  bool observing_system_tray_focus_ = false;

  bool shelf_enabled_ = true;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      observer_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_WEBUI_LOGIN_VIEW_H_
