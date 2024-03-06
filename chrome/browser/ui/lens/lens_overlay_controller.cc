// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_properties.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

LensOverlayController::LensOverlayController(tabs::TabModel* tab_model)
    : tab_model_(tab_model) {
  tab_model_->owning_model()->AddObserver(this);
}

LensOverlayController::~LensOverlayController() = default;

void LensOverlayController::ShowUI() {
  // If UI is already showing or in the process of showing, do nothing.
  if (state_ != State::kOff) {
    return;
  }

  // The UI should only show if the tab is in the foreground.
  if (tab_model_->owning_model()->GetActiveTab() != tab_model_) {
    return;
  }

  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view = tab_model_->contents()
                                            ->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();

  // During initialization and shutdown a capture may not be possible.
  if (!view || !view->IsSurfaceAvailableForCopy()) {
    return;
  }

  state_ = State::kScreenshot;
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindOnce(&LensOverlayController::DidCaptureScreenshot,
                     weak_factory_.GetWeakPtr(), ++screenshot_attempt_id_));
}

void LensOverlayController::DidCaptureScreenshot(int attempt_id,
                                                 const SkBitmap& bitmap) {
  // While capturing a screenshot the overlay was cancelled. Do nothing.
  if (state_ == State::kOff) {
    return;
  }

  // An id mismatch implies this is not the most recent screenshot attempt.
  if (screenshot_attempt_id_ != attempt_id) {
    return;
  }

  ShowOverlayWidget();

  state_ = State::kOverlay;
}

void LensOverlayController::ShowOverlayWidget() {
  CHECK(!overlay_widget_);

  // TODO(b/327270921): Implement on Mac.
#if !BUILDFLAG(IS_MAC)
  overlay_widget_ = std::make_unique<views::Widget>();
  overlay_widget_->Init(CreateWidgetInitParams());
  overlay_widget_->SetContentsView(CreateViewForOverlay());

  // Stack widget at top.
  auto* overlay_window = overlay_widget_->GetNativeWindow();
  auto* parent = overlay_window->parent();
  CHECK(parent);
  parent->StackChildAtTop(overlay_window);

  overlay_widget_->Show();
#endif
}

views::Widget::InitParams LensOverlayController::CreateWidgetInitParams() {
  content::WebContents* active_web_contents = tab_model_->contents();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "LensOverlayWidget";
  params.child = true;
#if !BUILDFLAG(IS_MAC)
  const gfx::NativeWindow& native_window =
      active_web_contents->GetTopLevelNativeWindow();
  params.parent = native_window;
  params.layer_type = ui::LAYER_NOT_DRAWN;
#endif
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = active_web_contents->GetContainerBounds();
  return params;
}

std::unique_ptr<views::View> LensOverlayController::CreateViewForOverlay() {
  CHECK(tab_model_);
  // Create a flex layout host view to make sure the web view covers the entire
  // tab.
  std::unique_ptr<views::FlexLayoutView> host_view =
      std::make_unique<views::FlexLayoutView>();

  // Create the web view that hosts the WebUI.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(tab_model_->owning_model()->profile());
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_view->GetWebContents(), SK_ColorTRANSPARENT);

  // Load the untrusted WebUI into the web view.
  GURL url(chrome::kChromeUILensUntrustedURL);
  web_view->LoadInitialURL(url);

  host_view->AddChildView(std::move(web_view));
  return host_view;
}

void LensOverlayController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }

  if (selection.new_contents == tab_model_->contents()) {
    TabForegrounded();
    return;
  }

  if (selection.old_contents == tab_model_->contents()) {
    TabBackgrounded();
  }
}

void LensOverlayController::TabForegrounded() {}

void LensOverlayController::TabBackgrounded() {
  CloseUI();
}

void LensOverlayController::CloseUI() {
  overlay_widget_.reset();
  // In the future we may want a hibernate state. In this case we would stop
  // showing the UI but persist enough information to defrost the original UI
  // state when the tab is foregrounded.
  state_ = State::kOff;
}

raw_ptr<views::Widget> LensOverlayController::GetOverlayWidgetForTesting() {
  return overlay_widget_.get();
}
