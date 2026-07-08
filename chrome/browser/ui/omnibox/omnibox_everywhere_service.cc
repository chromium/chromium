// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
bool IsAppActiveOnMac();
void HideAppOnMac();
void OrderOmniboxEverywhereFrontOnMac(views::Widget* widget);
#endif

namespace {

}  // namespace

OmniboxEverywhereService::OmniboxEverywhereService(Profile* profile)
    : profile_(profile) {
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere) &&
      ui::GlobalAcceleratorListener::GetInstance()) {
    ui::GlobalAcceleratorListener::GetInstance()->RegisterAccelerator(
        ui::Accelerator(ui::VKEY_SPACE,
                        ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR),
        this);
  }
}

OmniboxEverywhereService::~OmniboxEverywhereService() {
  Shutdown();
}

void OmniboxEverywhereService::Shutdown() {
  if (ui::GlobalAcceleratorListener::GetInstance()) {
    ui::GlobalAcceleratorListener::GetInstance()->UnregisterAccelerators(this);
  }
  widget_observation_.Reset();
  if (widget_) {
    widget_->CloseNow();
    widget_.reset();
  }
  contents_wrapper_.reset();
}

void OmniboxEverywhereService::TogglePopup() {
  if (IsPopupVisible()) {
    HidePopup();
  } else {
    CreateAndShowWidget();
  }
}

void OmniboxEverywhereService::HidePopup() {
  if (widget_) {
    widget_observation_.Reset();
    widget_->Hide();
#if BUILDFLAG(IS_MAC)
    if (!is_navigating_ && !was_active_before_popup_) {
      HideAppOnMac();
    }
#endif
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<OmniboxEverywhereService> service) {
                         if (service && service->widget_) {
                           service->widget_observation_.Reset();
                           service->widget_->CloseNow();
                           service->widget_.reset();
                           service->contents_wrapper_.reset();
                         }
                       },
                       weak_factory_.GetWeakPtr()));
  }
}

bool OmniboxEverywhereService::IsPopupVisible() const {
  return widget_ && widget_->IsVisible();
}

void OmniboxEverywhereService::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_key_press_time_.is_null() &&
      (now - last_key_press_time_) < base::Milliseconds(300)) {
    return;
  }
  last_key_press_time_ = now;

  // Only trigger for the last active profile or last used profile.
  BrowserWindowInterface* active_bwi =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  Browser* browser =
      active_bwi ? active_bwi->GetBrowserForMigrationOnly() : nullptr;
  Profile* target_profile = browser ? browser->GetProfile() : nullptr;
  if (!target_profile && g_browser_process->profile_manager()) {
    const std::vector<Profile*>& profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    if (!profiles.empty()) {
      target_profile = profiles[0];
    }
  }

  if (target_profile) {
    auto* service =
        OmniboxEverywhereServiceFactory::GetForProfile(target_profile);
    if (service != this) {
      return;
    }
#if BUILDFLAG(IS_MAC)
    service->SetWasActiveBeforePopup(IsAppActiveOnMac());
#else
    service->SetWasActiveBeforePopup(true);
#endif
    service->SetIsNavigating(false);
    service->TogglePopup();
  }
}

void OmniboxEverywhereService::ExecuteCommand(
    const std::string& accelerator_group_id,
    const std::string& command_id) {}

void OmniboxEverywhereService::CreateAndShowWidget() {
  if (!contents_wrapper_) {
    contents_wrapper_ =
        std::make_unique<WebUIContentsWrapperT<OmniboxEverywhereUI>>(
            GURL(chrome::kChromeUIOmniboxEverywhereURL), profile_,
            IDS_TASK_MANAGER_OMNIBOX);

    OmniboxPopupWebContentsHelper::CreateForWebContents(
        contents_wrapper_->web_contents());

    contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  }

  if (!widget_) {
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;

    gfx::Rect screen_bounds =
        display::Screen::Get()->GetPrimaryDisplay().bounds();
    gfx::Size popup_size(864, 632);

    params.bounds = gfx::Rect(
        screen_bounds.x() + (screen_bounds.width() - popup_size.width()) / 2,
        screen_bounds.y() + (screen_bounds.height() - popup_size.height()) / 2,
        popup_size.width(), popup_size.height());

    widget_->Init(std::move(params));

    auto web_view = std::make_unique<views::WebView>(profile_);
    web_view->SetWebContents(contents_wrapper_->web_contents());
    web_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
    if (auto* rwhv =
            contents_wrapper_->web_contents()->GetRenderWidgetHostView()) {
      rwhv->SetBackgroundColor(SK_ColorTRANSPARENT);
    }
    widget_->SetContentsView(std::move(web_view));

    widget_observation_.Observe(widget_.get());
  }

  widget_->Show();
#if BUILDFLAG(IS_MAC)
  OrderOmniboxEverywhereFrontOnMac(widget_.get());
#else
  widget_->Activate();
#endif

  if (widget_->GetContentsView()) {
    widget_->GetContentsView()->RequestFocus();
  }
  if (contents_wrapper_->web_contents()) {
    contents_wrapper_->web_contents()->Focus();
    if (auto* rwhv =
            contents_wrapper_->web_contents()->GetRenderWidgetHostView()) {
      rwhv->EnableAutoResize(gfx::Size(800, 50), gfx::Size(800, 800));
    }
  }
}

void OmniboxEverywhereService::OnWidgetActivationChanged(views::Widget* widget,
                                                         bool active) {
  if (!active) {
    HidePopup();
  }
}

void OmniboxEverywhereService::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  views::Widget* raw_widget = widget_.release();
  if (raw_widget) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                               raw_widget);
  }
  contents_wrapper_.reset();
}

void OmniboxEverywhereService::CloseUI() {
  HidePopup();
}

void OmniboxEverywhereService::ShowUI() {
  if (widget_) {
    widget_->Show();
#if BUILDFLAG(IS_MAC)
    OrderOmniboxEverywhereFrontOnMac(widget_.get());
#else
    widget_->Activate();
#endif
    if (widget_->GetContentsView()) {
      widget_->GetContentsView()->RequestFocus();
    }
    if (contents_wrapper_->web_contents()) {
      contents_wrapper_->web_contents()->Focus();
    }
  }
}

void OmniboxEverywhereService::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  if (widget_) {
    gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
    bounds.set_height(std::max(new_size.height() + 96, 56));
    widget_->SetBounds(bounds);
  }
}

void OmniboxEverywhereService::OpenUrl(const GURL& url,
                                       WindowOpenDisposition disposition,
                                       ui::PageTransition transition) {
  SetIsNavigating(true);
  HidePopup();

  BrowserWindowInterface* active_bwi =
      GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
  Browser* browser =
      active_bwi ? active_bwi->GetBrowserForMigrationOnly() : nullptr;
  if (browser && browser->GetProfile() != profile_) {
    browser = nullptr;
  }
  bool is_new_window = false;
  if (!browser) {
    browser = static_cast<Browser*>(chrome::OpenEmptyWindow(profile_));
    is_new_window = true;
  }
  if (browser) {
    NavigateParams params(browser, url, transition);
    params.disposition =
        is_new_window ? WindowOpenDisposition::CURRENT_TAB
                      : ((disposition == WindowOpenDisposition::CURRENT_TAB)
                             ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                             : disposition);
    params.window_action = NavigateParams::WindowAction::kShowWindow;
    Navigate(&params);
  }
}
