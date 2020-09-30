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
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Size of the bitmap saved.
constexpr gfx::Size kBitmapFinalSize(224, 64);
// Size of the WebContents used to render HTML.
constexpr gfx::Rect kWebContentsBounds(gfx::Point(), kBitmapFinalSize);

}  // namespace

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
  content::WebContents* web_contents = web_view_->GetWebContents();
  Observe(web_contents);
  // TODO(newcomer): Large items show a scrollbar, and small items do not need
  // this much room. Size the WebContents based on the required bounds.
  web_contents->GetNativeView()->SetBounds(kWebContentsBounds);
}

ClipboardImageModelRequest::~ClipboardImageModelRequest() = default;

void ClipboardImageModelRequest::Start(Params&& params) {
  DCHECK(!deliver_image_model_callback_);
  DCHECK(params.callback);
  DCHECK_EQ(base::UnguessableToken(), request_id_);

  request_id_ = std::move(params.id);
  deliver_image_model_callback_ = std::move(params.callback);

  timeout_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5), this,
                       &ClipboardImageModelRequest::OnTimeout);

  // Begin the document with the proper charset, this should prevent strange
  // looking characters from showing up in the render in some cases.
  std::string html_document(
      "<html><head><meta charset=\"UTF-8\"></meta></head><body>");
  // Hide overflow to prevent scroll bars from showing up, which occurs when the
  // rendered HTML takes up more space than 'kWebContentsBounds'.
  html_document.append("<style>body{overflow:hidden;}</style>");
  html_document.append(params.html_markup);
  html_document.append("</body></html>");

  std::string encoded_html;
  base::Base64Encode(html_document, &encoded_html);
  constexpr char kDataURIPrefix[] = "data:text/html;base64,";
  web_view_->GetWebContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL(kDataURIPrefix + encoded_html)));
  widget_->ShowInactive();
}

void ClipboardImageModelRequest::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  timeout_timer_.Stop();
  widget_->Hide();
  deliver_image_model_callback_.Reset();
  request_id_ = base::UnguessableToken();
  on_request_finished_callback_.Run();
}

bool ClipboardImageModelRequest::IsRunningRequest(
    base::Optional<base::UnguessableToken> request_id) const {
  return request_id.has_value() ? *request_id == request_id_
                                : !request_id_.is_empty();
}

void ClipboardImageModelRequest::DidStopLoading() {
  content::RenderWidgetHostView* source_view =
      web_view_->GetWebContents()->GetRenderViewHost()->GetWidget()->GetView();
  gfx::Size source_size = source_view->GetViewBounds().size();
  if (source_size.IsEmpty()) {
    Stop();
    return;
  }

  // There is no guarantee CopyFromSurface will call OnCopyComplete. If this
  // takes too long, this will be cleaned up by |timeout_timer_|.
  source_view->CopyFromSurface(
      gfx::Rect(source_size), kBitmapFinalSize,
      base::BindOnce(&ClipboardImageModelRequest::OnCopyComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardImageModelRequest::OnCopyComplete(const SkBitmap& bitmap) {
  std::move(deliver_image_model_callback_)
      .Run(ui::ImageModel::FromImageSkia(
          gfx::ImageSkia::CreateFrom1xBitmap(bitmap)));
  Stop();
}

void ClipboardImageModelRequest::OnTimeout() {
  DCHECK(deliver_image_model_callback_);
  Stop();
}
