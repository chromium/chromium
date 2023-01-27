// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commander_frontend_views.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
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
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {
constexpr gfx::Size kDefaultSize(512, 48);
constexpr int kTopContainerOverlapMargin = 12;
constexpr int kCornerRadius = 8;

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

// Helper to dismiss the commander widget on focus loss. This exists since
// `CommanderFrontendViews` is also a widget observer (but for the parent
// widget); splitting the responsibilities avoids the potential for awkward
// misunderstandings.
class CommanderFocusLossWatcher : public views::WidgetObserver {
 public:
  CommanderFocusLossWatcher(commander::CommanderFrontend* frontend,
                            views::Widget* widget)
      : frontend_(frontend) {
    widget_observation_.Observe(widget);
  }
  ~CommanderFocusLossWatcher() override = default;

  // views::WidgetObserver
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (!active)
      frontend_->Hide();
  }

 private:
  raw_ptr<commander::CommanderFrontend> frontend_;  // weak, owns us
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

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

  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override {
    return true;
  }

  void AddedToWidget() override {
    views::WebView::AddedToWidget();
    holder()->SetCornerRadii(gfx::RoundedCornersF(kCornerRadius));
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
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* owner_ = nullptr;
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
  subscription_ = browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
      &CommanderFrontendViews::OnAppTerminating, base::Unretained(this)));
}

CommanderFrontendViews::~CommanderFrontendViews() {
  backend_->SetUpdateCallback(base::DoNothing());
  if (widget_)
    widget_->CloseNow();
}
void CommanderFrontendViews::ToggleForBrowser(Browser* browser) {
  DCHECK(browser);
  // This ensures that quick commands are only available for normal browsers.
  if (!browser->is_type_normal())
    return;
  bool should_show = !browser_ || browser != browser_;
  if (browser_)
    Hide();
  if (should_show)
    Show(browser);
}

void CommanderFrontendViews::Show(Browser* browser) {
  DCHECK(!is_showing());
  DCHECK_EQ(nullptr, web_view_.get());
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
  params.name = "Quick Commands";
  params.parent = parent->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
// On Windows, this defaults to DesktopNativeWidgetAura, which has incorrect
// parenting behavior for this widget.
#if BUILDFLAG(IS_WIN)
  params.native_widget = new views::NativeWidgetAura(widget_);
#endif
  widget_->Init(std::move(params));
  focus_loss_watcher_ =
      std::make_unique<CommanderFocusLossWatcher>(this, widget_);

  auto web_view = CreateWebView(browser->profile());
  web_view->SetOwner(parent);
  web_view->SetSize(kDefaultSize);
  CommanderUI* controller = static_cast<CommanderUI*>(
      web_view->GetWebContents()->GetWebUI()->GetController());
  controller->handler()->PrepareToShow(this);

  web_view_ = widget_->SetContentsView(std::move(web_view));

  gfx::Rect bounds;
  bounds.set_size(kDefaultSize);
  AnchorToBrowser(&bounds, browser_);
  widget_->SetBounds(bounds);

  widget_->Show();

  web_view_->RequestFocus();
  web_view_->GetWebContents()->Focus();
}

void CommanderFrontendViews::Hide() {
  DCHECK(is_showing());

  BrowserView::GetBrowserViewForBrowser(browser_)->GetWidget()->RemoveObserver(
      this);
  BrowserList::RemoveObserver(this);
  backend_->Reset();
  browser_ = nullptr;

  widget_->GetRootView()->RemoveChildViewT(std::exchange(web_view_, nullptr));

  focus_loss_watcher_.reset();
  widget_delegate_->SetOwnedByWidget(true);
  std::ignore = widget_delegate_.release();
  widget_->Close();
  widget_ = nullptr;
}

void CommanderFrontendViews::OnBrowserClosing(Browser* browser) {
  if (browser_ == browser)
    Hide();
}

void CommanderFrontendViews::OnAppTerminating() {
  if (is_showing())
    Hide();
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
  web_view_->SetSize(size);
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

std::unique_ptr<CommanderWebView> CommanderFrontendViews::CreateWebView(
    Profile* profile) {
  DCHECK(profile);
  auto web_view = std::make_unique<CommanderWebView>(profile);
  web_view->set_allow_accelerators(true);
  // Make the commander WebContents show up in the task manager.
  content::WebContents* web_contents = web_view->GetWebContents();
  task_manager::WebContentsTags::CreateForToolContents(
      web_contents, IDS_QUICK_COMMANDS_LABEL);
  web_view->LoadInitialURL(GURL(chrome::kChromeUICommanderURL));
  return web_view;
}

// static
std::unique_ptr<commander::CommanderFrontend>
commander::CommanderFrontend::Create(commander::CommanderBackend* backend) {
  return std::make_unique<CommanderFrontendViews>(backend);
}
