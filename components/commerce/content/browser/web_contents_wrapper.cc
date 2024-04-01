// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/web_contents_wrapper.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"

namespace commerce {

WebContentsWrapper::WebContentsWrapper(content::WebContents* web_contents,
                                       int32_t js_world_id)
    : web_contents_(web_contents), js_world_id_(js_world_id) {}

WebContentsWrapper::~WebContentsWrapper() = default;

const GURL& WebContentsWrapper::GetLastCommittedURL() {
  if (!web_contents_)
    return GURL::EmptyGURL();

  return web_contents_->GetLastCommittedURL();
}

const std::u16string& WebContentsWrapper::GetTitle() {
  return web_contents_ ? web_contents_->GetTitle() : base::EmptyString16();
}

bool WebContentsWrapper::IsFirstLoadForNavigationFinished() {
  return is_first_load_for_nav_finished_;
}

void WebContentsWrapper::SetIsFirstLoadForNavigationFinished(bool finished) {
  is_first_load_for_nav_finished_ = finished;
}

bool WebContentsWrapper::IsOffTheRecord() {
  if (!web_contents_ || !web_contents_->GetBrowserContext())
    return false;

  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsWrapper::RunJavascript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value)> callback) {
  if (!web_contents_ || !web_contents_->GetPrimaryMainFrame()) {
    std::move(callback).Run(base::Value());
    return;
  }

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      script, std::move(callback), js_world_id_);
}

ukm::SourceId WebContentsWrapper::GetPageUkmSourceId() {
  if (!web_contents_ || !web_contents_->GetPrimaryMainFrame()) {
    return ukm::kInvalidSourceId;
  }
  return web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

void WebContentsWrapper::ClearWebContentsPointer() {
  web_contents_ = nullptr;
}

content::RenderFrameHost* WebContentsWrapper::GetPrimaryMainFrame() {
  if (!web_contents_) {
    return nullptr;
  }
  return web_contents_->GetPrimaryMainFrame();
}

}  // namespace commerce
