// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_web_dialog.h"

#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::content::WebContents;
using ::content::WebUIMessageHandler;

constexpr gfx::Insets kMinMargins{64};
constexpr gfx::Size kMinSize{128, 128};
constexpr gfx::Size kMaxSize{512, 512};

base::LazyInstance<base::circular_deque<WebContents*>>::DestructorAtExit
    g_web_contents_stack = LAZY_INSTANCE_INITIALIZER;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, public:

LoginWebDialog::LoginWebDialog(content::BrowserContext* browser_context,
                               gfx::NativeWindow parent_window,
                               const std::u16string& title,
                               const GURL& url)
    : browser_context_(browser_context), parent_window_(parent_window) {
  if (!parent_window_ && LoginDisplayHost::default_host()) {
    parent_window_ = LoginDisplayHost::default_host()->GetNativeWindow();
  }
  LOG_IF(WARNING, !parent_window_)
      << "No parent window. Dialog sizes could be wrong";

  // Shift-Back is how the Chromebox for Meetings "Hangup" button is encoded.
  RegisterAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_SHIFT_DOWN),
                      base::BindRepeating(&LoginWebDialog::MaybeCloseWindow,
                                          base::Unretained(this)));
  RegisterOnDialogClosedCallback(
      base::BindOnce(&LoginWebDialog::OnDialogClosing, base::Unretained(this)));
  set_dialog_modal_type(ui::mojom::ModalType::kSystem);
  set_dialog_content_url(url);
  set_minimum_dialog_size(kMinSize);
  set_dialog_title(title);
  set_show_dialog_title(true);
  set_allow_default_context_menu(false);
  set_allow_web_contents_creation(false);
}

LoginWebDialog::~LoginWebDialog() {}

void LoginWebDialog::Show() {
  dialog_window_ =
      chrome::ShowWebDialog(parent_window_, browser_context_, this);
}

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, protected:

void LoginWebDialog::GetDialogSize(gfx::Size* size) const {
  // TODO(crbug.com/40657776): Fix for the lock screen.
  if (!parent_window_) {
    *size = kMaxSize;
    return;
  }
  gfx::Rect bounds = parent_window_->bounds();
  bounds.Inset(kMinMargins);
  *size = bounds.size();
  size->SetToMin(kMaxSize);
  size->SetToMax(kMinSize);
}

// static.
WebContents* LoginWebDialog::GetCurrentWebContents() {
  auto& stack = g_web_contents_stack.Get();
  return stack.empty() ? nullptr : stack.front();
}

void LoginWebDialog::OnDialogShown(content::WebUI* webui) {
  g_web_contents_stack.Pointer()->push_front(webui->GetWebContents());
}

void LoginWebDialog::OnDialogClosing(const std::string& json_retval) {
  dialog_window_ = nullptr;
}

void LoginWebDialog::OnCloseContents(WebContents* source,
                                     bool* out_close_dialog) {
  *out_close_dialog = true;

  if (GetCurrentWebContents() == source) {
    g_web_contents_stack.Pointer()->pop_front();
  }
}

bool LoginWebDialog::HandleOpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback,
    WebContents** out_new_contents) {
  // On a login screen, if a missing extension is trying to show in a web
  // dialog, a NetErrorHelper is displayed instead (hence we have a `source`),
  // but there is no browser window associated with it. A helper screen will
  // fire an auto-reload, which in turn leads to opening a new browser window,
  // so we must suppress it.
  // http://crbug.com/443096
  return (source && !chrome::FindBrowserWithTab(source));
}

bool LoginWebDialog::MaybeCloseWindow(WebDialogDelegate& delegate,
                                      const ui::Accelerator& accelerator) {
  if (!dialog_window_) {
    return false;
  }

  views::Widget::GetWidgetForNativeWindow(dialog_window_)->Close();
  return true;
}

}  // namespace ash
