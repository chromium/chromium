// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_navigation_button_container.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/hit_test_utils.h"

namespace {

constexpr int kPaddingBetweenNavigationButtons = 5;

#if BUILDFLAG(IS_CHROMEOS)
constexpr int kWebAppFrameLeftMargin = 2;
#else
constexpr int kWebAppFrameLeftMargin = 7;
#endif

}  // namespace

WebAppNavigationButtonContainer::WebAppNavigationButtonContainer(
    BrowserView* browser_view,
    ToolbarButtonProvider* toolbar_button_provider)
    : browser_(browser_view->browser()) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(0, kWebAppFrameLeftMargin),
          kPaddingBetweenNavigationButtons));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout.set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  back_button_ = AddChildView(std::make_unique<BackForwardButton>(
      BackForwardButton::Direction::kBack,
      base::BindRepeating(
          [](Browser* browser, const ui::Event& event) {
            chrome::ExecuteCommandWithDisposition(
                browser, IDC_BACK,
                ui::DispositionFromEventFlags(event.flags()));
          },
          browser_),
      browser_));
  back_button_->set_tag(IDC_BACK);
#if BUILDFLAG(IS_WIN)
  back_button_->SetVectorIcons(kBackArrowWindowsIcon,
                               kBackArrowWindowsTouchIcon);
#endif

  ConfigureWebAppToolbarButton(back_button_, toolbar_button_provider);
  views::SetHitTestComponent(back_button_, static_cast<int>(HTCLIENT));
  chrome::AddCommandObserver(browser_, IDC_BACK, this);

  const auto* app_controller = browser_->app_controller();
  if (app_controller->HasReloadButton()) {
    reload_button_ = AddChildView(
        std::make_unique<ReloadButton>(browser_->command_controller()));
    reload_button_->set_tag(IDC_RELOAD);
#if BUILDFLAG(IS_WIN)
    reload_button_->SetVectorIconsForMode(ReloadButton::Mode::kReload,
                                          kReloadWindowsIcon,
                                          kReloadWindowsTouchIcon);
    reload_button_->SetVectorIconsForMode(ReloadButton::Mode::kStop,
                                          kNavigateStopWindowsIcon,
                                          kNavigateStopWindowsTouchIcon);
#endif

    ConfigureWebAppToolbarButton(reload_button_, toolbar_button_provider);
    views::SetHitTestComponent(reload_button_, static_cast<int>(HTCLIENT));
    chrome::AddCommandObserver(browser_, IDC_RELOAD, this);
  }
}

WebAppNavigationButtonContainer::~WebAppNavigationButtonContainer() {
  chrome::RemoveCommandObserver(browser_, IDC_BACK, this);
  if (reload_button_) {
    chrome::RemoveCommandObserver(browser_, IDC_RELOAD, this);
  }
}

BackForwardButton* WebAppNavigationButtonContainer::back_button() {
  return back_button_;
}

ReloadButton* WebAppNavigationButtonContainer::reload_button() {
  return reload_button_;
}

void WebAppNavigationButtonContainer::EnabledStateChangedForCommand(
    int id,
    bool enabled) {
  switch (id) {
    case IDC_BACK:
      back_button_->SetEnabled(enabled);
      break;
    case IDC_RELOAD:
      DCHECK(reload_button_);
      reload_button_->SetEnabled(enabled);
      break;
    default:
      NOTREACHED();
  }
}

BEGIN_METADATA(WebAppNavigationButtonContainer)
END_METADATA
