// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commander_frontend_views.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/commander_backend.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/commander/commander_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_service.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {
constexpr gfx::Size kDefaultSize(512, 48);
constexpr int kTopContainerOverlapMargin = 12;

//
void AnchorToBrowser(gfx::Rect* bounds, Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  gfx::Rect top_container_bounds =
      browser_view->top_container()->GetBoundsInScreen();

  bounds->set_x(top_container_bounds.x() +
                (top_container_bounds.width() - bounds->width()) / 2);
  bounds->set_y(top_container_bounds.bottom() - kTopContainerOverlapMargin);
}

}  // namespace

// A small shim to handle passing keyboard events back up to the browser.
// Required for hotkeys to work.
class CommanderWebView : public views::WebView {
 public:
  METADATA_HEADER(CommanderWebView);
  explicit CommanderWebView(content::BrowserContext* context)
      : views::WebView(context) {}
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    CHECK(owner_);
    return event_handler_.HandleKeyboardEvent(event, owner_->GetFocusManager());
  }

  void SetOwner(views::View* owner) {
    if (owner_ == owner)
      return;
    owner_ = owner;
    OnPropertyChanged(&owner_, views::kPropertyEffectsNone);
  }
  views::View* GetOwner() const { return owner_; }

 private:
  views::UnhandledKeyboardEventHandler event_handler_;
  views::View* owner_ = nullptr;
};

BEGIN_METADATA(CommanderWebView, views::WebView)
ADD_PROPERTY_METADATA(views::View*, Owner)
END_METADATA

CommanderFrontendViews::CommanderFrontendViews(
    commander::CommanderBackend* backend)
    : backend_(backend) {
  backend_->SetUpdateCallback(
      base::BindRepeating(&CommanderFrontendViews::OnViewModelUpdated,
                          weak_ptr_factory_.GetWeakPtr()));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->CreateProfileAsync(
      ProfileManager::GetSystemProfilePath(),
      base::BindRepeating(&CommanderFrontendViews::OnSystemProfileAvailable,
                          weak_ptr_factory_.GetWeakPtr()));
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
void CommanderFrontendViews::ToggleForBrowser(Browser* browser) {
  DCHECK(browser);
  bool should_show = !browser_ || browser != browser_;
  if (browser_)
    Hide();
  if (should_show)
    Show(browser);
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
  BrowserList::AddObserver(this);
  views::View* parent = BrowserView::GetBrowserViewForBrowser(browser_);
  widget_delegate_ = std::make_unique<views::WidgetDelegate>();
  widget_delegate_->SetCanActivate(true);
  views::Widget* parent_widget = parent->GetWidget();
  parent_widget->AddObserver(this);
  widget_ = new ThemeCopyingWidget(parent_widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = widget_delegate_.get();
  params.name = "Commander";
  params.parent = parent->GetWidget()->GetNativeView();
// On Windows, this defaults to DesktopNativeWidgetAura, which has incorrect
// parenting behavior for this widget.
#if defined(OS_WIN)
  params.native_widget = new views::NativeWidgetAura(widget_);
#endif
  widget_->Init(std::move(params));

  web_view_->SetOwner(parent);
  web_view_->SetSize(kDefaultSize);
  CommanderUI* controller = static_cast<CommanderUI*>(
      web_view_->GetWebContents()->GetWebUI()->GetController());
  controller->handler()->PrepareToShow(this);

  web_view_ptr_ = widget_->SetContentsView(std::move(web_view_));

  gfx::Rect bounds;
  bounds.set_size(kDefaultSize);
  AnchorToBrowser(&bounds, browser_);
  widget_->SetBounds(bounds);

  widget_->Show();

  web_view_ptr_->RequestFocus();
  web_view_ptr_->GetWebContents()->Focus();
}

void CommanderFrontendViews::Hide() {
  DCHECK(is_showing());

  BrowserView::GetBrowserViewForBrowser(browser_)->GetWidget()->RemoveObserver(
      this);
  BrowserList::RemoveObserver(this);
  backend_->Reset();
  show_requested_ = false;
  browser_ = nullptr;

  web_view_ = widget_->GetRootView()->RemoveChildViewT(web_view_ptr_);
  web_view_->SetOwner(nullptr);

  widget_delegate_->SetOwnedByWidget(true);
  ignore_result(widget_delegate_.release());
  widget_->Close();
  widget_ = nullptr;
}

void CommanderFrontendViews::OnBrowserClosing(Browser* browser) {
  if (browser_ == browser)
    Hide();
}

void CommanderFrontendViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  if (is_showing())
    Hide();
  web_view_->SetWebContents(nullptr);
}

void CommanderFrontendViews::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  DCHECK(browser_);
  DCHECK(is_showing());
  DCHECK(widget ==
         BrowserView::GetBrowserViewForBrowser(browser_)->GetWidget());
  gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
  AnchorToBrowser(&bounds, browser_);
  widget_->SetBounds(bounds);
}

void CommanderFrontendViews::OnTextChanged(const std::u16string& text) {
  DCHECK(is_showing());
  backend_->OnTextChanged(text, browser_);
}

void CommanderFrontendViews::OnOptionSelected(size_t option_index,
                                              int result_set_id) {
  DCHECK(is_showing());
  backend_->OnCommandSelected(option_index, result_set_id);
}

void CommanderFrontendViews::OnCompositeCommandCancelled() {
  DCHECK(is_showing());
  backend_->OnCompositeCommandCancelled();
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
      web_view_ptr_->GetWebContents()->GetWebUI()->GetController());
  controller->handler()->ViewModelUpdated(std::move(view_model));
  // TODO(lgrey): Pass view model to WebUI.
}

void CommanderFrontendViews::OnSystemProfileAvailable(
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CreateStatus::CREATE_STATUS_INITIALIZED &&
      !is_showing()) {
    CreateWebView(profile);
  }
}

void CommanderFrontendViews::CreateWebView(Profile* profile) {
  DCHECK(!is_web_view_created());

  web_view_ = std::make_unique<CommanderWebView>(profile);
  web_view_->set_allow_accelerators(true);
  // Make the commander WebContents show up in the task manager.
  content::WebContents* web_contents = web_view_->GetWebContents();
  task_manager::WebContentsTags::CreateForToolContents(web_contents,
                                                       IDS_COMMANDER_LABEL);
  web_view_->LoadInitialURL(GURL(chrome::kChromeUICommanderURL));
  if (show_requested_)
    Show(browser_);
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

// static
std::unique_ptr<commander::CommanderFrontend>
commander::CommanderFrontend::Create(commander::CommanderBackend* backend) {
  return std::make_unique<CommanderFrontendViews>(backend);
}
