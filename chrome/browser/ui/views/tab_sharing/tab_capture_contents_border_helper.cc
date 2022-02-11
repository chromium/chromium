// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"

#include <limits>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/gfx/color_palette.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
const int kContentsBorderThickness = 5;
const float kContentsBorderOpacity = 0.50;
const SkColor kContentsBorderColor = gfx::kGoogleBlue500;
#endif

// TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
void InitContentsBorderWidget(content::WebContents* web_contents) {
  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    return;
  }

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || browser_view->contents_border_widget()) {
    return;
  }

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  views::Widget* frame = browser_view->contents_web_view()->GetWidget();
  params.parent = frame->GetNativeView();
  params.context = frame->GetNativeWindow();
  // Make the widget non-top level.
  params.child = true;
  params.name = "TabSharingContentsBorder";
  params.remove_standard_frame = true;
  // Let events go through to underlying view.
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
#if BUILDFLAG(IS_WIN)
  params.native_widget = new views::NativeWidgetAura(widget);
#endif  // BUILDFLAG(IS_WIN)

  widget->Init(std::move(params));
  auto border_view = std::make_unique<views::View>();
  border_view->SetBorder(
      views::CreateSolidBorder(kContentsBorderThickness, kContentsBorderColor));
  widget->SetContentsView(std::move(border_view));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->SetOpacity(kContentsBorderOpacity);

  browser_view->set_contents_border_widget(widget);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

void TabCaptureContentsBorderHelper::IncrementCapturerCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_LT(capturer_count_, std::numeric_limits<int>::max());

  ++capturer_count_;
  Update();
}

void TabCaptureContentsBorderHelper::DecrementCapturerCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_GT(capturer_count_, 0);

  // TODO(crbug.com/1294187): Destroy widget when `capturer_count_` hits 0.
  --capturer_count_;
  Update();
}

void TabCaptureContentsBorderHelper::VisibilityUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Update();
}

void TabCaptureContentsBorderHelper::Update() {
// TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* const web_contents = &GetWebContents();

  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    return;
  }

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }

  const bool tab_visible =
      (web_contents == browser->tab_strip_model()->GetActiveWebContents());
  const bool contents_border_needed = tab_visible && capturer_count_ > 0;

  if (!browser_view->contents_border_widget()) {
    if (!contents_border_needed) {
      return;
    }
    InitContentsBorderWidget(web_contents);
  }

  views::Widget* const contents_border_widget =
      browser_view->contents_border_widget();

  if (contents_border_needed) {
    contents_border_widget->Show();
  } else {
    contents_border_widget->Hide();
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabCaptureContentsBorderHelper);
