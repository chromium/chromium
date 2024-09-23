// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_web_dialog_view.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_view.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chrome {
namespace {

gfx::NativeWindow CreateWebDialogWidget(views::Widget::InitParams params,
                                        views::WebDialogView* view,
                                        bool show = true) {
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));

  if (show)
    widget->Show();
  return widget->GetNativeWindow();
}

}  // namespace

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate,
                                bool show) {
  return ShowWebDialogWithParams(parent, context, delegate, std::nullopt, show);
}

gfx::NativeWindow ShowWebDialogWithParams(
    gfx::NativeView parent,
    content::BrowserContext* context,
    ui::WebDialogDelegate* delegate,
    std::optional<views::Widget::InitParams> extra_params,
    bool show) {
  views::WebDialogView* view = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  view = new ash::SystemWebDialogView(
      context, delegate, std::make_unique<ChromeWebContentsHandler>());
#else
  view = new views::WebDialogView(context, delegate,
                                  std::make_unique<ChromeWebContentsHandler>());
#endif

  // If the corner radius is specified, set it to |views::DialogDelegate|.
  if (extra_params && extra_params->corner_radius)
    view->set_corner_radius(*(extra_params->corner_radius));

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  if (extra_params)
    params = std::move(*extra_params);
  params.delegate = view;
  params.parent = parent;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!parent &&
      delegate->GetDialogModalType() == ui::mojom::ModalType::kSystem) {
    int container_id = ash_util::GetSystemModalDialogContainerId();
    ash_util::SetupWidgetInitParamsForContainer(&params, container_id);
  }
#endif
  gfx::NativeWindow window =
      CreateWebDialogWidget(std::move(params), view, show);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user = ash::ProfileHelper::Get()->GetUserByProfile(
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

}  // namespace chrome
