// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_image_model_request.h"

#include <memory>

#include "base/base64.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

ClipboardImageModelRequest::Params::Params(const base::UnguessableToken& id,
                                           const std::string& html_markup,
                                           ImageModelCallback callback)
    : id(id), html_markup(html_markup), callback(std::move(callback)) {}

ClipboardImageModelRequest::Params::Params(Params&&) = default;

ClipboardImageModelRequest::Params&
ClipboardImageModelRequest::Params::operator=(Params&&) = default;

ClipboardImageModelRequest::Params::~Params() = default;

ClipboardImageModelRequest::ClipboardImageModelRequest(
    Profile* profile,
    base::RepeatingClosure on_request_finished_callback)
    : widget_(std::make_unique<views::Widget>()),
      web_view_(new views::WebView(profile)),
      on_request_finished_callback_(std::move(on_request_finished_callback)) {
  views::Widget::InitParams widget_params;
  widget_params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.name = "ClipboardImageModelRequest";
  widget_->Init(std::move(widget_params));
  widget_->SetContentsView(web_view_);

  Observe(web_view_->GetWebContents());
  web_contents()->SetDelegate(this);
}

ClipboardImageModelRequest::~ClipboardImageModelRequest() = default;

void ClipboardImageModelRequest::Start(Params&& params) {
  DCHECK(!deliver_image_model_callback_);
  DCHECK(params.callback);
  DCHECK_EQ(base::UnguessableToken(), request_id_);

  request_id_ = std::move(params.id);
  deliver_image_model_callback_ = std::move(params.callback);

  timeout_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(10), this,
                       &ClipboardImageModelRequest::OnTimeout);

  // Begin the document with the proper charset, this should prevent strange
  // looking characters from showing up in the render in some cases.
  std::string html_document(
      "<!DOCTYPE html><html><head><meta "
      "charset=\"UTF-8\"></meta></head><body>");
  html_document.append(params.html_markup);
  html_document.append("</body></html>");

  std::string encoded_html;
  base::Base64Encode(html_document, &encoded_html);
  constexpr char kDataURIPrefix[] = "data:text/html;base64,";
  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL(kDataURIPrefix + encoded_html)));
  widget_->ShowInactive();
}

void ClipboardImageModelRequest::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  copy_surface_weak_ptr_factory_.InvalidateWeakPtrs();
  timeout_timer_.Stop();
  widget_->Hide();
  deliver_image_model_callback_.Reset();
  request_id_ = base::UnguessableToken();
  did_auto_resize_ = false;
  on_request_finished_callback_.Run();
}

bool ClipboardImageModelRequest::IsRunningRequest(
    base::Optional<base::UnguessableToken> request_id) const {
  return request_id.has_value() ? *request_id == request_id_
                                : !request_id_.is_empty();
}

void ClipboardImageModelRequest::ResizeDueToAutoResize(
    content::WebContents* web_contents,
    const gfx::Size& new_size) {
  did_auto_resize_ = true;
  web_contents->GetNativeView()->SetBounds(gfx::Rect(gfx::Point(), new_size));

  // `ResizeDueToAutoResize()` can be called before and/or after
  // DidStopLoading(). If `DidStopLoading()` has not been called, wait for the
  // next resize before copying the surface.
  if (!web_contents->IsLoading())
    PostCopySurfaceTask();
}

void ClipboardImageModelRequest::DidStopLoading() {
  // Wait for auto resize. In some cases the data url will stop loading before
  // auto resize has occurred. This will result in a incorrectly sized image.
  if (!did_auto_resize_)
    return;

  PostCopySurfaceTask();
}

void ClipboardImageModelRequest::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  if (!web_contents()->GetRenderWidgetHostView())
    return;

  web_contents()->GetRenderWidgetHostView()->EnableAutoResize(
      gfx::Size(1, 1), gfx::Size(INT_MAX, INT_MAX));
}

void ClipboardImageModelRequest::PostCopySurfaceTask() {
  if (!deliver_image_model_callback_)
    return;

  // Debounce calls to `CopySurface()`. `DidStopLoading()` and
  // `ResizeDueToAutoResize()` can be called multiple times in the same task
  // sequence. Wait for the final update before copying the surface.
  copy_surface_weak_ptr_factory_.InvalidateWeakPtrs();
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClipboardImageModelRequest::CopySurface,
                     copy_surface_weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(100));
}

void ClipboardImageModelRequest::CopySurface() {
  content::RenderWidgetHostView* source_view =
      web_contents()->GetRenderViewHost()->GetWidget()->GetView();
  if (source_view->GetViewBounds().size().IsEmpty()) {
    Stop();
    return;
  }

  // There is no guarantee CopyFromSurface will call OnCopyComplete. If this
  // takes too long, this will be cleaned up by |timeout_timer_|.
  source_view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindOnce(&ClipboardImageModelRequest::OnCopyComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardImageModelRequest::OnCopyComplete(const SkBitmap& bitmap) {
  if (!deliver_image_model_callback_) {
    Stop();
    return;
  }

  std::move(deliver_image_model_callback_)
      .Run(ui::ImageModel::FromImageSkia(
          gfx::ImageSkia::CreateFrom1xBitmap(bitmap)));
  Stop();
}

void ClipboardImageModelRequest::OnTimeout() {
  DCHECK(deliver_image_model_callback_);
  Stop();
}
