// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {
constexpr int kCustomizationBubbleHeight = 515;
constexpr int kCustomizationBubbleWidth = 290;
}  // namespace

ProfileCustomizationBubbleView::~ProfileCustomizationBubbleView() = default;

// static
ProfileCustomizationBubbleView* ProfileCustomizationBubbleView::CreateBubble(
    Profile* profile,
    views::View* anchor_view) {
  ProfileCustomizationBubbleView* bubble_view =
      new ProfileCustomizationBubbleView(profile, anchor_view);
  // The widget is owned by the views system.
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  // TODO(droger): Delay showing the bubble until the web view is loaded.
  widget->Show();
  return bubble_view;
}

ProfileCustomizationBubbleView::ProfileCustomizationBubbleView(
    Profile* profile,
    views::View* anchor_view)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT) {
  set_close_on_deactivate(false);
  // Create the web view in the native bubble.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(profile);
  web_view->LoadInitialURL(GURL(chrome::kChromeUIProfileCustomizationURL));
  web_view->SetPreferredSize(
      gfx::Size(kCustomizationBubbleWidth, kCustomizationBubbleHeight));
  ProfileCustomizationUI* web_ui = web_view->GetWebContents()
                                       ->GetWebUI()
                                       ->GetController()
                                       ->GetAs<ProfileCustomizationUI>();
  SetInitiallyFocusedView(web_view.get());
  DCHECK(web_ui);
  web_ui->Initialize(
      base::BindOnce(&ProfileCustomizationBubbleView::OnDoneButtonClicked,
                     // Unretained is fine because this owns the web view.
                     base::Unretained(this)));
  AddChildView(std::move(web_view));

  set_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);  // Buttons are implemented in WebUI.
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void ProfileCustomizationBubbleView::OnDoneButtonClicked() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      GetAnchorView()->GetWidget()->GetNativeWindow());
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  browser_view->MaybeShowProfileSwitchIPH();
}

BEGIN_METADATA(ProfileCustomizationBubbleView, views::BubbleDialogDelegateView)
END_METADATA

void DiceWebSigninInterceptorDelegate::ShowProfileCustomizationBubbleInternal(
    Browser* browser) {
  DCHECK(browser);

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAvatarToolbarButton();
  DCHECK(anchor_view);
  ProfileCustomizationBubbleView::CreateBubble(browser->profile(), anchor_view);
}
