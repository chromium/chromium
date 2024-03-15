// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_query_controller.h"
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
#include "content/public/browser/web_contents_user_data.h"
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

// When a WebUIController for lens overlay is created, we need a mechanism to
// glue that instance to the LensOverlayController that spawned it. This class
// is that glue. The lifetime of this instance is scoped to the lifetime of the
// LensOverlayController, which semantically "owns" this instance.
class LensOverlayControllerGlue
    : public content::WebContentsUserData<LensOverlayControllerGlue> {
 public:
  ~LensOverlayControllerGlue() override = default;

  LensOverlayController* controller() { return controller_; }

 private:
  friend WebContentsUserData;

  LensOverlayControllerGlue(content::WebContents* contents,
                            LensOverlayController* controller)
      : content::WebContentsUserData<LensOverlayControllerGlue>(*contents),
        controller_(controller) {}

  // Semantically owns this class.
  raw_ptr<LensOverlayController> controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(LensOverlayControllerGlue);

}  // namespace

LensOverlayController::LensOverlayController(tabs::TabModel* tab_model)
    : tab_model_(tab_model) {
  // Automatically unregisters on destruction.
  tab_model_->owning_model()->AddObserver(this);
  lens_overlay_query_controller_ =
      std::make_unique<lens::LensOverlayQueryController>();
}

LensOverlayController::~LensOverlayController() {
  CloseUI();
  lens_overlay_query_controller_.reset();
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController, kOverlayId);

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
            tab_browser, this,
            SidePanelUI::GetSidePanelUIForBrowser(tab_browser),
            tab_model_->contents());
  }

  state_ = State::kScreenshot;
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindOnce(&LensOverlayController::DidCaptureScreenshot,
                     weak_factory_.GetWeakPtr(), ++screenshot_attempt_id_));
}

void LensOverlayController::CloseUI() {
  // Destroy the glue to avoid UaF. This must be done before destroying
  // `results_side_panel_coordinator_` or `overlay_widget_`.
  // This logic results on the assumption that the only way to destroy the
  // instances of views::WebView being glued is through this method. Any changes
  // to this assumption will likely need to restructure the concept of
  // `glued_webviews_`.
  while (!glued_webviews_.empty()) {
    RemoveGlueForWebView(glued_webviews_.front());
  }
  glued_webviews_.clear();

  results_side_panel_coordinator_.reset();

  // Widget destruction can be asynchronous. We want to synchronously release
  // resources, so we clear the contents view immediately.
  if (overlay_widget_) {
    overlay_widget_->SetContentsView(std::make_unique<views::View>());
  }
  overlay_widget_.reset();
  tab_contents_observer_.reset();

  side_panel_receiver_.reset();
  side_panel_page_.reset();
  receiver_.reset();
  page_.reset();
  current_screenshot_.reset();
  // In the future we may want a hibernate state. In this case we would stop
  // showing the UI but persist enough information to defrost the original UI
  // state when the tab is foregrounded.
  state_ = State::kOff;
}

// static
LensOverlayController* LensOverlayController::GetController(
    content::WebUI* web_ui) {
  return LensOverlayControllerGlue::FromWebContents(web_ui->GetWebContents())
      ->controller();
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

void LensOverlayController::BindSidePanel(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensSidePanelPage> page) {
  // If a side panel was already bound to this overlay controller, then we
  // should reset. This can occur if the side panel is closed and then reopened
  // while the overlay is open.
  side_panel_receiver_.reset();
  side_panel_page_.reset();

  side_panel_receiver_.Bind(std::move(receiver));
  side_panel_page_.Bind(std::move(page));
}

views::Widget* LensOverlayController::GetOverlayWidgetForTesting() {
  return overlay_widget_.get();
}

void LensOverlayController::ResetUIBounds() {
  content::WebContents* active_web_contents = tab_model_->contents();
  overlay_widget_->SetBounds(active_web_contents->GetContainerBounds());
}

void LensOverlayController::CreateGlueForWebView(views::WebView* web_view) {
  LensOverlayControllerGlue::CreateForWebContents(web_view->GetWebContents(),
                                                  this);
  glued_webviews_.push_back(web_view);
}

void LensOverlayController::RemoveGlueForWebView(views::WebView* web_view) {
  auto it = std::find(glued_webviews_.begin(), glued_webviews_.end(), web_view);
  if (it != glued_webviews_.end()) {
    web_view->GetWebContents()->RemoveUserData(
        LensOverlayControllerGlue::UserDataKey());
    glued_webviews_.erase(it);
  }
}

class LensOverlayController::UnderlyingWebContentsObserver
    : public content::WebContentsObserver {
 public:
  UnderlyingWebContentsObserver(content::WebContents* web_contents,
                                LensOverlayController* lens_overlay_controller)
      : content::WebContentsObserver(web_contents),
        lens_overlay_controller_(lens_overlay_controller) {}

  ~UnderlyingWebContentsObserver() override = default;

  UnderlyingWebContentsObserver(const UnderlyingWebContentsObserver&) = delete;
  UnderlyingWebContentsObserver& operator=(
      const UnderlyingWebContentsObserver&) = delete;

  // content::WebContentsObserver
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override {
    // We only care to resize the overlay when it's visible to the user.
    if (lens_overlay_controller_->state() == State::kStartingWebUI ||
        lens_overlay_controller_->state() == State::kOverlay ||
        lens_overlay_controller_->state() == State::kOverlayAndResults) {
      lens_overlay_controller_->ResetUIBounds();
    }
  }

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override {
    lens_overlay_controller_->CloseUIAsync();
  }

 private:
  raw_ptr<LensOverlayController> lens_overlay_controller_;
};

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

  // It is not possible to show the overlay UI if the tab is not associated with
  // a tab strip.
  if (!tab_model_->owning_model()) {
    CloseUI();
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

  content::WebContents* active_web_contents = tab_model_->contents();
  tab_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
      active_web_contents, this);

  // Stack widget at top.
  gfx::NativeWindow top_level_native_window =
      active_web_contents->GetTopLevelNativeWindow();
  views::Widget* top_level_widget =
      views::Widget::GetWidgetForNativeWindow(top_level_native_window);
  overlay_widget_->StackAboveWidget(top_level_widget);

  overlay_widget_->Show();
}

views::Widget::InitParams LensOverlayController::CreateWidgetInitParams() {
  content::WebContents* active_web_contents = tab_model_->contents();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "LensOverlayWidget";
  params.child = true;

  gfx::NativeWindow top_level_native_window =
      active_web_contents->GetTopLevelNativeWindow();
  views::Widget* top_level_widget =
      views::Widget::GetWidgetForNativeWindow(top_level_native_window);
  gfx::NativeView top_level_native_view = top_level_widget->GetNativeView();
  params.parent = top_level_native_view;
  params.layer_type = ui::LAYER_NOT_DRAWN;

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
  web_view->SetProperty(views::kElementIdentifierKey, kOverlayId);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_view->GetWebContents(), SK_ColorTRANSPARENT);

  // Create glue so that WebUIControllers created by this instance can
  // communicate with this instance.
  CreateGlueForWebView(web_view.get());

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

void LensOverlayController::CloseRequestedByOverlay() {
  CloseUIAsync();
}

void LensOverlayController::CloseUIAsync() {
  state_ = State::kClosing;

  // This callback comes from WebUI. CloseUI synchronously destroys the WebUI.
  // Dispatch to avoid re-entrancy.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LensOverlayController::CloseUI,
                                weak_factory_.GetWeakPtr()));
}

void LensOverlayController::IssueLensRequest(const ::gfx::RectF& region) {
  // TODO(b/328255310): Use region to build an actual request. For now, just
  // open side panel.
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}
