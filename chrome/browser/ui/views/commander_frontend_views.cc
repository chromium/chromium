// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commander_frontend_views.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/commander/commander_backend.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/commander/commander_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// TODO(lgrey): Temporary
constexpr gfx::Size kDefaultSize(400, 30);
}  // namespace

// A small shim to handle passing keyboard events back up to the browser.
// Required for hotkeys to work.
class CommanderWebView : public views::WebView {
 public:
  explicit CommanderWebView(content::BrowserContext* context)
      : views::WebView(context) {}
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    CHECK(owner_);
    return event_handler_.HandleKeyboardEvent(event, owner_->GetFocusManager());
  }

  void set_owner(views::View* owner) { owner_ = owner; }

 private:
  views::UnhandledKeyboardEventHandler event_handler_;
  views::View* owner_;
};

CommanderFrontendViews::CommanderFrontendViews(
    commander::CommanderBackend* backend)
    : backend_(backend) {
  widget_delegate_ = std::make_unique<views::WidgetDelegate>();
  widget_delegate_->SetCanActivate(true);
  widget_delegate_->RegisterWindowClosingCallback(
      base::BindRepeating(&CommanderFrontendViews::OnWindowClosing,
                          weak_ptr_factory_.GetWeakPtr()));

  backend_->SetUpdateCallback(
      base::BindRepeating(&CommanderFrontendViews::OnViewModelUpdated,
                          weak_ptr_factory_.GetWeakPtr()));

#if !defined(OS_CHROMEOS)
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->CreateProfileAsync(
      ProfileManager::GetSystemProfilePath(),
      base::BindRepeating(&CommanderFrontendViews::OnSystemProfileAvailable,
                          weak_ptr_factory_.GetWeakPtr()),
      base::string16(), std::string());
#else
  // TODO(lgrey): ChromeOS doesn't have a system profile. Need to find
  // a better way to do this before Commander is hooked up, but doing
  // this for now to unblock.
  CreateWebView(ProfileManager::GetPrimaryUserProfile());
#endif
}

CommanderFrontendViews::~CommanderFrontendViews() {
  backend_->SetUpdateCallback(base::DoNothing());
  if (widget_)
    widget_->CloseNow();
}

void CommanderFrontendViews::Show(Browser* browser) {
  if (!is_web_view_created()) {
    browser_ = browser;
    show_requested_ = true;
    return;
  }
  DCHECK(!is_showing());
  show_requested_ = false;
  browser_ = browser;
  views::View* parent = BrowserView::GetBrowserViewForBrowser(browser_);

  widget_ = new ThemeCopyingWidget(parent->GetWidget());
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = widget_delegate_.get();
  params.name = "Commander";
  params.parent = parent->GetWidget()->GetNativeView();
  widget_->Init(std::move(params));

  web_view_->set_owner(parent);
  web_view_->SetSize(kDefaultSize);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUICommanderURL));
  CommanderUI* controller = static_cast<CommanderUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  controller->handler()->set_delegate(this);

  web_view_ptr_ = widget_->SetContentsView(std::move(web_view_));

  widget_->CenterWindow(kDefaultSize);
  widget_->Show();

  web_view_ptr_->RequestFocus();
  web_view_ptr_->GetWebContents()->Focus();
}

void CommanderFrontendViews::Hide() {
  DCHECK(is_showing());
  widget_->Close();
}

void CommanderFrontendViews::OnWindowClosing() {
  DCHECK(is_showing());
  backend_->Reset();
  web_view_ = widget_->GetRootView()->RemoveChildViewT(web_view_ptr_);
  web_view_->set_owner(nullptr);
  show_requested_ = false;
  browser_ = nullptr;
  widget_ = nullptr;
}

void CommanderFrontendViews::OnTextChanged(const base::string16& text) {
  DCHECK(is_showing());
  backend_->OnTextChanged(text, browser_);
}

void CommanderFrontendViews::OnOptionSelected(size_t option_index,
                                              int result_set_id) {
  DCHECK(is_showing());
  backend_->OnCommandSelected(option_index, result_set_id);
}

void CommanderFrontendViews::OnDismiss() {
  Hide();
}

void CommanderFrontendViews::OnHeightChanged(int new_height) {
  DCHECK(is_showing());
  gfx::Size size = kDefaultSize;
  size.set_height(new_height);
  widget_->SetSize(size);
  web_view_ptr_->SetSize(size);
}

void CommanderFrontendViews::OnHandlerEnabled(bool is_enabled) {
  is_handler_enabled_ = is_enabled;
}

void CommanderFrontendViews::OnViewModelUpdated(
    commander::CommanderViewModel view_model) {
  DCHECK(is_showing());
  if (view_model.action == commander::CommanderViewModel::Action::kClose) {
    Hide();
    return;
  }
  if (!is_handler_enabled_)
    // TODO(lgrey): Think through whether it makes sense to stash the view model
    // and send it when the handler becomes available again.
    return;
  CommanderUI* controller = static_cast<CommanderUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  controller->handler()->ViewModelUpdated(std::move(view_model));
  // TODO(lgrey): Pass view model to WebUI.
}

void CommanderFrontendViews::OnSystemProfileAvailable(
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CreateStatus::CREATE_STATUS_CREATED && !is_showing())
    CreateWebView(profile);
}

void CommanderFrontendViews::CreateWebView(Profile* profile) {
  DCHECK(!is_web_view_created());

  web_view_ = std::make_unique<CommanderWebView>(profile);
  web_view_->set_allow_accelerators(true);
  if (show_requested_)
    Show(browser_);
}

// static
std::unique_ptr<commander::CommanderFrontend>
commander::CommanderFrontend::Create(commander::CommanderBackend* backend) {
  return std::make_unique<CommanderFrontendViews>(backend);
}
