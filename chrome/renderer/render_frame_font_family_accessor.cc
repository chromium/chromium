// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_frame_font_family_accessor.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"

namespace {

std::vector<std::string> WebStringVectorToStl(
    const blink::WebVector<blink::WebString>& web_vector) {
  std::vector<std::string> stl_vector;
  for (const blink::WebString& web_string : web_vector)
    stl_vector.push_back(web_string.Utf8());
  return stl_vector;
}

}  // namespace

// static
void RenderFrameFontFamilyAccessor::Bind(
    content::RenderFrame* render_frame,
    mojo::PendingAssociatedReceiver<
        chrome::mojom::RenderFrameFontFamilyAccessor> pending_receiver) {
  new RenderFrameFontFamilyAccessor(render_frame, std::move(pending_receiver));
}

RenderFrameFontFamilyAccessor::RenderFrameFontFamilyAccessor(
    content::RenderFrame* render_frame,
    mojo::PendingAssociatedReceiver<
        chrome::mojom::RenderFrameFontFamilyAccessor> pending_receiver)
    : RenderFrameObserver(render_frame),
      receiver_(this, std::move(pending_receiver)) {
  // While unlikely, it is possible the fonts were requested after fcp. If this
  // happens copy the fonts now.
  if (ShouldGetFontNames())
    GetFontNames();
}

RenderFrameFontFamilyAccessor::~RenderFrameFontFamilyAccessor() {
  // Mojo requires the receiver to be destroyed before the callback, otherwise
  // a DCHECK will be hit.
  receiver_.reset();
}

bool RenderFrameFontFamilyAccessor::ShouldGetFontNames() const {
  return !render_frame()
              ->GetWebFrame()
              ->PerformanceMetricsForReporting()
              .FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime()
              .is_null();
}

void RenderFrameFontFamilyAccessor::GetFontNames() {
  family_names_ = render_frame()->GetWebFrame()->GetWebFontFamilyNames();
}

void RenderFrameFontFamilyAccessor::GetFontFamilyNames(
    GetFontFamilyNamesCallback callback) {
  if (got_font_names()) {
    RunCallback(std::move(callback));
  } else {
    // Browser side only requests once per interface.
    DCHECK(!callback_);
    callback_ = std::move(callback);
  }
}

void RenderFrameFontFamilyAccessor::OnDestruct() {
  delete this;
}

void RenderFrameFontFamilyAccessor::DidChangePerformanceTiming() {
  if (!got_commit_ || got_font_names() || !ShouldGetFontNames())
    return;

  GetFontNames();
  if (callback_)
    RunCallback(std::move(callback_));
}

void RenderFrameFontFamilyAccessor::ReadyToCommitNavigation(
    blink::WebDocumentLoader* docuqment_loader) {
  if (got_commit_) {
    // This is the second time ReadyToCommitNavigation() has been called.
    // This means the renderer has started loading a different page than the
    // fonts were originally requested for.
    delete this;
  } else {
    got_commit_ = true;
  }
}

void RenderFrameFontFamilyAccessor::RunCallback(
    GetFontFamilyNamesCallback callback) {
  std::move(callback).Run(WebStringVectorToStl(family_names_->font_names));
}
