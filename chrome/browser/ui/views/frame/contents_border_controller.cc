// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_border_controller.h"

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {
class BorderView : public views::View {
 public:
  BorderView() = default;
  BorderView(const BorderView&) = delete;
  BorderView& operator=(const BorderView&) = delete;
  ~BorderView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    constexpr int kContentsBorderThickness = 5;
    SetBorder(views::CreateSolidBorder(
        kContentsBorderThickness,
        GetColorProvider()->GetColor(kColorCapturedTabContentsBorder)));
  }
};
}  // namespace

ContentsBorderController::ContentsBorderController(BrowserView* browser_view)
    : browser_view_(browser_view) {
  active_tab_change_subscription_ =
      browser_view->browser()->RegisterActiveTabDidChange(
          base::BindRepeating(&ContentsBorderController::OnActiveTabChanged,
                              base::Unretained(this)));
}

ContentsBorderController::~ContentsBorderController() = default;

void ContentsBorderController::AboutToBeDiscarded(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

void ContentsBorderController::InitializeBorderWidget() {
  capture_content_border_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  views::Widget* frame = browser_view_->contents_web_view()->GetWidget();
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
  params.native_widget =
      new views::NativeWidgetAura(capture_content_border_widget_.get());
#endif  // BUILDFLAG(IS_WIN)

  capture_content_border_widget_->Init(std::move(params));
  capture_content_border_widget_->SetContentsView(
      std::make_unique<BorderView>());
  capture_content_border_widget_->SetVisibilityChangedAnimationsEnabled(false);
  capture_content_border_widget_->SetOpacity(0.50f);
  browser_view_->set_contents_border_widget(
      capture_content_border_widget_.get());
}

void ContentsBorderController::OnActiveTabChanged(
    BrowserWindowInterface* browser_window_interface) {
  tabs::TabInterface* const tab_interface =
      browser_window_interface->GetActiveTabInterface();
  TabCaptureContentsBorderHelper* const contents_border_helper =
      TabCaptureContentsBorderHelper::FromWebContents(
          tab_interface->GetContents());
  CHECK(contents_border_helper);
  Observe(tab_interface->GetContents());
  tab_capture_change_subscription_ =
      contents_border_helper->AddOnTabCaptureChangeCallback(
          base::BindRepeating(&ContentsBorderController::OnTabCaptureChange,
                              base::Unretained(this)));
  const bool is_tab_capturing = contents_border_helper->IsTabCapturing();
  OnTabCaptureChange(is_tab_capturing,
                     is_tab_capturing
                         ? contents_border_helper->GetBlueBorderLocation()
                         : std::nullopt);
}

void ContentsBorderController::OnTabCaptureChange(
    bool is_capturing,
    std::optional<gfx::Rect> border_location) {
  if (!browser_view_->contents_border_widget()) {
    if (is_capturing) {
      InitializeBorderWidget();
    } else {
      return;
    }
  }

  browser_view_->contents_border_widget()->SetVisible(is_capturing);
  if (is_capturing) {
    browser_view_->SetContentBorderBounds(border_location);
  }
}
