// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_ui.h"
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

namespace {

// In order to glue the WebUIController to the appropriate instance of
// LensOverlayController, we need to keep a global list of a
// LensOverlayControllers.
using ControllerVector = std::vector<LensOverlayController*>;
ControllerVector& GetAllControllers() {
  static base::NoDestructor<ControllerVector> instance;
  return *instance;
}

}  // namespace

LensOverlayController::LensOverlayController(tabs::TabModel* tab_model)
    : tab_model_(tab_model) {
  // Automatically unregisters on destruction.
  tab_model_->owning_model()->AddObserver(this);

  GetAllControllers().push_back(this);
}

LensOverlayController::~LensOverlayController() {
  std::erase(GetAllControllers(), this);
}

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

  // Create the results side panel coordinator when showing the UI if it does
  // not already exist for this tab's web contents.
  if (!results_side_panel_coordinator_) {
    Browser* tab_browser = chrome::FindBrowserWithTab(tab_model_->contents());
    CHECK(tab_browser);
    results_side_panel_coordinator_ =
        std::make_unique<lens::LensOverlaySidePanelCoordinator>(
            tab_browser, SidePanelUI::GetSidePanelUIForBrowser(tab_browser),
            tab_model_->contents());
  }

  state_ = State::kScreenshot;
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindOnce(&LensOverlayController::DidCaptureScreenshot,
                     weak_factory_.GetWeakPtr(), ++screenshot_attempt_id_));
}

void LensOverlayController::CloseUI() {
  results_side_panel_coordinator_.reset();
  overlay_widget_.reset();
  overlay_web_contents_ = nullptr;
  receiver_.reset();
  page_.reset();
  current_screenshot_.reset();
  // In the future we may want a hibernate state. In this case we would stop
  // showing the UI but persist enough information to defrost the original UI
  // state when the tab is foregrounded.
  state_ = State::kOff;
}

// static
void LensOverlayController::BindOverlay(
    content::WebUI* web_ui,
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  content::WebContents* web_contents = web_ui->GetWebContents();
  for (LensOverlayController* controller : GetAllControllers()) {
    if (controller->overlay_web_contents_ == web_contents) {
      controller->BindOverlay(std::move(receiver), std::move(page));
      return;
    }
  }
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

  // Need to store the current screenshot before creating the WebUI, since the
  // WebUI is dependent on the screenshot.
  current_screenshot_ = bitmap;
  ShowOverlayWidget();

  state_ = State::kStartingWebUI;
}

void LensOverlayController::ShowOverlayWidget() {
  CHECK(!overlay_widget_);

  overlay_widget_ = std::make_unique<views::Widget>();
  overlay_widget_->Init(CreateWidgetInitParams());
  overlay_widget_->SetContentsView(CreateViewForOverlay());

  // Stack widget at top.
#if BUILDFLAG(IS_MAC)
  content::WebContents* active_web_contents = tab_model_->contents();
  const gfx::NativeView web_contents_view =
      active_web_contents->GetContentNativeView();
  overlay_widget_->StackAbove(web_contents_view);
#else
  auto* overlay_window = overlay_widget_->GetNativeWindow();
  auto* parent = overlay_window->parent();
  CHECK(parent);
  parent->StackChildAtTop(overlay_window);
#endif
  overlay_widget_->Show();
}

views::Widget::InitParams LensOverlayController::CreateWidgetInitParams() {
  content::WebContents* active_web_contents = tab_model_->contents();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "LensOverlayWidget";
  params.child = true;
#if BUILDFLAG(IS_MAC)
  const gfx::NativeView web_contents_view =
      active_web_contents->GetContentNativeView();
  params.parent = web_contents_view;
#else
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
  overlay_web_contents_ = web_view->GetWebContents();

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

void LensOverlayController::BindOverlay(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  if (state_ != State::kStartingWebUI) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  page_.Bind(std::move(page));
  state_ = State::kOverlay;
}

void LensOverlayController::CloseRequestedByOverlay() {
  CloseUI();
}

raw_ptr<views::Widget> LensOverlayController::GetOverlayWidgetForTesting() {
  return overlay_widget_.get();
}
