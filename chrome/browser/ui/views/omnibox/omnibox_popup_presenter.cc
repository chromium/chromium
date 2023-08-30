// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/realbox/realbox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                             OmniboxController* controller)
    : views::WebView(location_bar_view->profile()),
      location_bar_view_(location_bar_view),
      widget_(nullptr),
      waited_for_handler_(false) {
  set_owned_by_client();

  // Prepare for instantiation of a `RealboxHandler` that will connect with
  // this omnibox controller. The URL load will instantiate and bind
  // the handler asynchronously.
  OmniboxPopupUI::SetOmniboxController(controller);
  LoadInitialURL(GURL(chrome::kChromeUIOmniboxPopupURL));
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() {
  ReleaseWidget(false);
}

void OmniboxPopupPresenter::Show() {
  if (!widget_) {
    widget_ = new ThemeCopyingWidget(location_bar_view_->GetWidget());

    views::Widget* parent_widget = location_bar_view_->GetWidget();
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
#if BUILDFLAG(IS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread during command buffer creation. See http://crbug.com/125248
    params.force_software_compositing = true;
#endif
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.parent = parent_widget->GetNativeView();
    params.context = parent_widget->GetNativeWindow();

    RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_);

    widget_->Init(std::move(params));

    widget_->ShowInactive();

    widget_->SetContentsView(
        std::make_unique<RoundedOmniboxResultsFrame>(this, location_bar_view_));
    widget_->AddObserver(this);
    FrameSizeChanged(nullptr, gfx::Size(0, 0));
  }
}

void OmniboxPopupPresenter::Hide() {
  // Only close if UI DevTools settings allow.
  if (widget_ && widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
    ReleaseWidget(true);
  }
}

bool OmniboxPopupPresenter::IsShown() const {
  return !!widget_;
}

RealboxHandler* OmniboxPopupPresenter::GetHandler() {
  if (!waited_for_handler_) {
    waited_for_handler_ = true;
    WaitForHandler();
  }
  OmniboxPopupUI* omnibox_popup_ui = static_cast<OmniboxPopupUI*>(
      GetWebContents()->GetWebUI()->GetController());
  CHECK(IsHandlerReady());
  return omnibox_popup_ui->handler();
}

// TODO(crbug.com/1396174): This should also be called when LocationBarView
//  size is changed.
void OmniboxPopupPresenter::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (widget_) {
    gfx::Rect widget_bounds = location_bar_view_->GetBoundsInScreen();
    widget_bounds.Inset(
        -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());

    // The width is known, and is the basis for consistent web content rendering
    // so width is specified exactly; then only height adjusts dynamically.
    // TODO(crbug.com/1396174): Change max height according to max suggestion
    //  count and calculated row height, or use a more general maximum value.
    const int width = widget_bounds.width();
    constexpr int kMaxHeight = 480;
    EnableSizingFromWebContents(gfx::Size(width, 1),
                                gfx::Size(width, kMaxHeight));

    widget_bounds.set_height(widget_bounds.height() +
                             std::min(kMaxHeight, frame_size.height()));
    widget_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
    widget_->SetBounds(widget_bounds);
  }
}

void OmniboxPopupPresenter::OnWidgetDestroyed(views::Widget* widget) {
  // TODO(crbug.com/1445142): Consider restoring if not closed logically by
  // omnibox.
  if (widget == widget_) {
    widget_ = nullptr;
  }
}

void OmniboxPopupPresenter::WaitForHandler() {
  bool ready = IsHandlerReady();
  base::UmaHistogramBoolean("Omnibox.WebUI.HandlerReady", ready);
  if (!ready) {
    SCOPED_UMA_HISTOGRAM_TIMER("Omnibox.WebUI.HandlerWait");
    base::RunLoop loop;
    auto quit = loop.QuitClosure();
    auto runner = base::ThreadPool::CreateTaskRunner(base::TaskTraits());
    runner->PostTask(FROM_HERE,
                     base::BindOnce(&OmniboxPopupPresenter::WaitInternal,
                                    weak_ptr_factory_.GetWeakPtr(), &quit));
    loop.Run();
    CHECK(IsHandlerReady());
  }
}

void OmniboxPopupPresenter::WaitInternal(base::RepeatingClosure* closure) {
  while (!IsHandlerReady()) {
    base::PlatformThread::Sleep(base::Milliseconds(1));
  }
  closure->Run();
}

bool OmniboxPopupPresenter::IsHandlerReady() {
  OmniboxPopupUI* omnibox_popup_ui = static_cast<OmniboxPopupUI*>(
      GetWebContents()->GetWebUI()->GetController());
  return omnibox_popup_ui->handler() &&
         omnibox_popup_ui->handler()->IsRemoteBound();
}

void OmniboxPopupPresenter::ReleaseWidget(bool close) {
  if (widget_) {
    // Avoid possibility of dangling raw_ptr by nulling before cleanup.
    views::Widget* widget = widget_;
    widget_ = nullptr;

    widget->RemoveObserver(this);
    if (close) {
      widget->Close();
    }
  }
  CHECK(!IsInObserverList());
}

BEGIN_METADATA(OmniboxPopupPresenter, views::WebView)
END_METADATA
