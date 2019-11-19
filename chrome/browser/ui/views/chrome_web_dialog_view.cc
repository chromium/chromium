// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_web_dialog_view.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#endif  // defined(OS_CHROMEOS)

namespace chrome {
namespace {

gfx::NativeWindow CreateWebDialogWidget(views::Widget::InitParams params,
                                        views::WebDialogView* view,
                                        bool show = true) {
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));

  // Observer is needed for ChromeVox extension to send messages between content
  // and background scripts.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      view->web_contents());

  if (show)
    widget->Show();
  return widget->GetNativeWindow();
}

}  // namespace

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate) {
  return ShowWebDialogWithParams(parent, context, delegate, base::nullopt);
}

gfx::NativeWindow ShowWebDialogWithParams(
    gfx::NativeView parent,
    content::BrowserContext* context,
    ui::WebDialogDelegate* delegate,
    base::Optional<views::Widget::InitParams> extra_params) {
  views::WebDialogView* view = new views::WebDialogView(
      context, delegate, std::make_unique<ChromeWebContentsHandler>());
  views::Widget::InitParams params;
  if (extra_params)
    params = std::move(*extra_params);
  params.delegate = view;
  params.parent = parent;
#if defined(OS_CHROMEOS)
  if (!parent && delegate->GetDialogModalType() == ui::MODAL_TYPE_SYSTEM) {
    int container_id = ash_util::GetSystemModalDialogContainerId();
    ash_util::SetupWidgetInitParamsForContainer(&params, container_id);
  }
#endif
  gfx::NativeWindow window = CreateWebDialogWidget(std::move(params), view);
#if defined(OS_CHROMEOS)
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(context));
  if (user && session_manager::SessionManager::Get()->session_state() ==
                  session_manager::SessionState::ACTIVE) {
    // Dialogs should not be shown for other users when logged in and the
    // session is active.
    MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
        window, user->GetAccountId());
  }
#endif
  return window;
}

gfx::NativeWindow CreateWebDialogWithBounds(gfx::NativeView parent,
                                            content::BrowserContext* context,
                                            ui::WebDialogDelegate* delegate,
                                            const gfx::Rect& bounds,
                                            bool show) {
  // Use custom dialog frame instead of platform frame when possible.
  bool use_dialog_frame = views::DialogDelegate::CanSupportCustomFrame(parent);
  views::WebDialogView* view = new views::WebDialogView(
      context, delegate, std::make_unique<ChromeWebContentsHandler>(),
      use_dialog_frame);

  views::Widget::InitParams params;
  params.delegate = view;
  params.bounds = bounds;
  params.parent = parent;
  if (use_dialog_frame) {
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.remove_standard_frame = true;
#if !defined(OS_MACOSX)
    // Except on Mac, the bubble frame includes its own shadow; remove any
    // native shadowing. On Mac, the window server provides the shadow.
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
#endif
  }

  return CreateWebDialogWidget(std::move(params), view, show);
}

}  // namespace chrome
