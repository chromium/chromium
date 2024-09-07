// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard/clipboard_image_model_request.h"

#include <memory>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/scoped_clipboard_history_pause.h"
#include "base/base64.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// The maximum size that the web contents can be. It caps the memory consumption
// incurred by web contents rendering.
constexpr gfx::Size kMaxWebContentsSize(2000, 2000);

// The initial size of the NativeView to force painting in an inactive shown
// widget for auto-resize mode.
constexpr gfx::Size kAutoResizeModeInitialSize(1, 1);

ClipboardImageModelRequest::TestParams* g_test_params = nullptr;

}  // namespace

// ClipboardImageModelFactory::Params: -----------------------------------------

ClipboardImageModelRequest::Params::Params(const base::UnguessableToken& id,
                                           const std::string& html_markup,
                                           const gfx::Size& bounding_box_size,
                                           ImageModelCallback callback)
    : id(id),
      html_markup(html_markup),
      bounding_box_size(bounding_box_size),
      callback(std::move(callback)) {}

ClipboardImageModelRequest::Params::Params(Params&&) = default;

ClipboardImageModelRequest::Params&
ClipboardImageModelRequest::Params::operator=(Params&&) = default;

ClipboardImageModelRequest::Params::~Params() = default;

// ClipboardImageModelFactory::TestParams: -------------------------------------

ClipboardImageModelRequest::TestParams::TestParams(
    RequestStopCallback callback,
    const std::optional<bool>& enforce_auto_resize)
    : callback(callback), enforce_auto_resize(enforce_auto_resize) {}

ClipboardImageModelRequest::TestParams::~TestParams() = default;

// ClipboardImageModelRequest------------- -------------------------------------

ClipboardImageModelRequest::ScopedClipboardModifier::ScopedClipboardModifier(
    const std::string& html_markup) {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  const auto* current_data = clipboard->GetClipboardData(&data_dst);

  // No need to replace the clipboard contents if the markup is the same.
  if (current_data && (html_markup == current_data->markup_data()))
    return;

  // Put |html_markup| on the clipboard temporarily so it can be pasted into
  // the WebContents. This is preferable to directly loading |html_markup_| in a
  // data URL because pasting the data into WebContents sanitizes the markup.
  // TODO(crbug.com/40729185): Sanitize copied HTML prior to storing it
  // in the clipboard buffer. Then |html_markup_| can be loaded from a data URL
  // and will not need to be pasted in this manner.
  auto new_data = std::make_unique<ui::ClipboardData>();
  new_data->set_markup_data(html_markup);

  scoped_clipboard_history_pause_ =
      ash::ClipboardHistoryController::Get()->CreateScopedPause();
  replaced_clipboard_data_ = clipboard->WriteClipboardData(std::move(new_data));
}

ClipboardImageModelRequest::ScopedClipboardModifier::
    ~ScopedClipboardModifier() {
  if (!replaced_clipboard_data_)
    return;

  ui::ClipboardNonBacked::GetForCurrentThread()->WriteClipboardData(
      std::move(replaced_clipboard_data_));
}

ClipboardImageModelRequest::ClipboardImageModelRequest(
    Profile* profile,
    base::RepeatingClosure on_request_finished_callback)
    : widget_(std::make_unique<views::Widget>()),
      web_view_(new views::WebView(profile)),
      on_request_finished_callback_(std::move(on_request_finished_callback)),
      request_creation_time_(base::TimeTicks::Now()) {
  CHECK(profile);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.name = "ClipboardImageModelRequest";
  widget_->Init(std::move(widget_params));
  widget_->SetContentsView(web_view_);

  Observe(web_view_->GetWebContents());
  web_contents()->SetDelegate(this);
}

ClipboardImageModelRequest::~ClipboardImageModelRequest() {
  UMA_HISTOGRAM_TIMES("Ash.ClipboardHistory.ImageModelRequest.Lifetime",
                      base::TimeTicks::Now() - request_creation_time_);
}

void ClipboardImageModelRequest::Start(Params&& params) {
  DCHECK(!deliver_image_model_callback_);
  DCHECK(params.callback);
  DCHECK_EQ(base::UnguessableToken(), request_id_);

  request_id_ = std::move(params.id);
  html_markup_ = params.html_markup;
  deliver_image_model_callback_ = std::move(params.callback);

  timeout_timer_.Start(FROM_HERE, base::Seconds(10), this,
                       &ClipboardImageModelRequest::OnTimeout);
  request_start_time_ = base::TimeTicks::Now();

  // Begin the document with the proper charset, this should prevent strange
  // looking characters from showing up in the render in some cases.
  std::string html_document(
      "<!DOCTYPE html>"
      "<html>"
      " <head><meta charset=\"UTF-8\"></meta></head>"
      " <body contenteditable='true' style=\"overflow: hidden\"> "
      "  <script>"
      // Focus the Contenteditable body to ensure WebContents::Paste() reaches
      // the body.
      "   document.body.focus();"
      "  </script>"
      " </body>"
      "</html");

  std::string encoded_html = base::Base64Encode(html_document);
  constexpr char kDataURIPrefix[] = "data:text/html;base64,";
  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL(kDataURIPrefix + encoded_html)));
  widget_->ShowInactive();

  // Adapt to the render widget host view whose device scale factor is not one.
  bounding_box_size_ = gfx::ScaleToCeiledSize(
      params.bounding_box_size,
      web_contents()->GetRenderWidgetHostView()->GetDeviceScaleFactor());
}

void ClipboardImageModelRequest::Stop(RequestStopReason stop_reason) {
  UMA_HISTOGRAM_ENUMERATION("Ash.ClipboardHistory.ImageModelRequest.StopReason",
                            stop_reason);
  DCHECK(!request_start_time_.is_null());
  UMA_HISTOGRAM_TIMES("Ash.ClipboardHistory.ImageModelRequest.Runtime",
                      base::TimeTicks::Now() - request_start_time_);
  request_start_time_ = base::TimeTicks();
  scoped_clipboard_modifier_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  copy_surface_weak_ptr_factory_.InvalidateWeakPtrs();
  timeout_timer_.Stop();
  widget_->Hide();
  deliver_image_model_callback_.Reset();
  request_id_ = base::UnguessableToken();
  did_stop_loading_ = false;

  on_request_finished_callback_.Run();

  if (g_test_params && g_test_params->callback)
    g_test_params->callback.Run(ShouldEnableAutoResizeMode());
}

ClipboardImageModelRequest::Params
ClipboardImageModelRequest::StopAndGetParams() {
  DCHECK(IsRunningRequest());
  Params params(request_id_, html_markup_, bounding_box_size_,
                std::move(deliver_image_model_callback_));
  Stop(RequestStopReason::kRequestCanceled);
  return params;
}

bool ClipboardImageModelRequest::IsModifyingClipboard() const {
  return scoped_clipboard_modifier_.has_value();
}

bool ClipboardImageModelRequest::IsRunningRequest(
    std::optional<base::UnguessableToken> request_id) const {
  return request_id.has_value() ? *request_id == request_id_
                                : !request_id_.is_empty();
}

void ClipboardImageModelRequest::ResizeDueToAutoResize(
    content::WebContents* web_contents,
    const gfx::Size& new_size) {
  web_contents->GetNativeView()->SetBounds(gfx::Rect(gfx::Point(), new_size));

  // `ResizeDueToAutoResize()` can be called before and/or after
  // DidStopLoading(). If `DidStopLoading()` has not been called, wait for the
  // next resize before copying the surface.
  if (!web_contents->IsLoading())
    PostCopySurfaceTask();
}

void ClipboardImageModelRequest::DidStopLoading() {
  // `DidStopLoading()` can be called multiple times after a paste. We are only
  // interested in the initial load of the data URL.
  if (did_stop_loading_)
    return;

  did_stop_loading_ = true;

  // Modify the clipboard so `html_markup_` can be pasted into the WebContents.
  scoped_clipboard_modifier_.emplace(html_markup_);

  web_contents()->GetRenderViewHost()->GetWidget()->InsertVisualStateCallback(
      base::BindOnce(&ClipboardImageModelRequest::OnVisualStateChangeFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  // After navigating to a new page, the surface id is invalidated. As a result,
  // copy from surface is disabled as well. Setting the window's bounds should
  // generate a new local surface id. Hence, window bounds setting should be
  // after the web navigation.
  // Changing auto resize mode does not generate a new local surface id while
  // setting the window bounds will. As a result, enabling/disabling the auto
  // resize mode has to precede the window bounds setting. Otherwise the change
  // in the window bounds may trigger the unnecessary update in the view layout
  // for the obsolete auto resize state. This layout update will consume the
  // newly generated local surface id. Then it will cause a crash when the
  // layout update brought by the change in the auto resize state arrives.
  if (ShouldEnableAutoResizeMode()) {
    web_contents()->GetRenderWidgetHostView()->EnableAutoResize(
        kAutoResizeModeInitialSize, kMaxWebContentsSize);
    web_contents()->GetNativeView()->SetBounds(
        gfx::Rect(kAutoResizeModeInitialSize));
  } else {
    web_contents()->GetRenderWidgetHostView()->DisableAutoResize(
        bounding_box_size_);
    web_contents()->GetNativeView()->SetBounds(gfx::Rect(bounding_box_size_));
  }

  // TODO(https://crbug.com/1149556): Clipboard Contents could be overwritten
  // prior to the `WebContents::Paste()` completing.
  web_contents()->Paste();
}

// static
void ClipboardImageModelRequest::SetTestParams(TestParams* test_params) {
  // Supports only setting `g_test_params` or resetting it.
  DCHECK(!g_test_params || !test_params);
  g_test_params = test_params;
}

void ClipboardImageModelRequest::OnVisualStateChangeFinished(bool done) {
  if (!done)
    return;

  scoped_clipboard_modifier_.reset();
  PostCopySurfaceTask();
}

void ClipboardImageModelRequest::PostCopySurfaceTask() {
  if (!deliver_image_model_callback_)
    return;

  // Debounce calls to `CopySurface()`. `DidStopLoading()` and
  // `ResizeDueToAutoResize()` can be called multiple times in the same task
  // sequence. Wait for the final update before copying the surface.
  copy_surface_weak_ptr_factory_.InvalidateWeakPtrs();
  DCHECK(
      web_contents()->GetRenderWidgetHostView()->IsSurfaceAvailableForCopy());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClipboardImageModelRequest::CopySurface,
                     copy_surface_weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(250));
}

void ClipboardImageModelRequest::CopySurface() {
  content::RenderWidgetHostView* source_view =
      web_contents()->GetRenderViewHost()->GetWidget()->GetView();
  if (source_view->GetViewBounds().size().IsEmpty()) {
    Stop(RequestStopReason::kEmptyResult);
    return;
  }

  // There is no guarantee CopyFromSurface will call OnCopyComplete. If this
  // takes too long, this will be cleaned up by |timeout_timer_|.
  source_view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindOnce(&ClipboardImageModelRequest::OnCopyComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     source_view->GetDeviceScaleFactor()));
}

void ClipboardImageModelRequest::OnCopyComplete(float device_scale_factor,
                                                const SkBitmap& bitmap) {
  if (!deliver_image_model_callback_) {
    Stop(RequestStopReason::kMultipleCopyCompletion);
    return;
  }

  std::move(deliver_image_model_callback_)
      .Run(ui::ImageModel::FromImageSkia(
          gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, device_scale_factor))));
  Stop(RequestStopReason::kFulfilled);
}

void ClipboardImageModelRequest::OnTimeout() {
  DCHECK(deliver_image_model_callback_);
  Stop(RequestStopReason::kTimeout);
}

bool ClipboardImageModelRequest::ShouldEnableAutoResizeMode() const {
  // Prefer to use the auto resize mode specified by `g_test_params` if any.
  if (g_test_params && g_test_params->enforce_auto_resize)
    return *g_test_params->enforce_auto_resize;

  // Use auto resize mode if `bounding_box_size_` is not meaningful.
  if (bounding_box_size_.IsEmpty())
    return true;

  // Use auto resize mode if the copied web content is too big to render.
  return !gfx::Rect(kMaxWebContentsSize)
              .Contains(gfx::Rect(bounding_box_size_));
}
