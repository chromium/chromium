// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/ios/browser/web_state_wrapper.h"

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/web_state.h"

namespace commerce {

WebStateWrapper::WebStateWrapper(web::WebState* web_state)
    : web_state_(web_state) {}

WebStateWrapper::~WebStateWrapper() = default;

const GURL& WebStateWrapper::GetLastCommittedURL() {
  if (!web_state_)
    return GURL::EmptyGURL();

  return web_state_->GetLastCommittedURL();
}

const std::u16string& WebStateWrapper::GetTitle() {
  return web_state_ ? web_state_->GetTitle() : base::EmptyString16();
}

bool WebStateWrapper::IsFirstLoadForNavigationFinished() {
  return is_first_load_for_nav_finished_;
}

void WebStateWrapper::SetIsFirstLoadForNavigationFinished(bool finished) {
  is_first_load_for_nav_finished_ = finished;
}

bool WebStateWrapper::IsOffTheRecord() {
  if (!web_state_ || !web_state_->GetBrowserState())
    return false;

  return web_state_->GetBrowserState()->IsOffTheRecord();
}

void WebStateWrapper::RunJavascript(
    const std::u16string& script,
    base::OnceCallback<void(const base::Value)> callback) {
  // GetPageWorldWebFramesManager() never returns null, but the main frame mght
  // be.
  if (!web_state_ ||
      !web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame()) {
    std::move(callback).Run(base::Value());
    return;
  }

  web_state_->GetPageWorldWebFramesManager()
      ->GetMainWebFrame()
      ->ExecuteJavaScript(
          script, base::BindOnce(
                      [](base::OnceCallback<void(const base::Value)> callback,
                         const base::Value* response) {
                        std::move(callback).Run(response ? response->Clone()
                                                         : base::Value());
                      },
                      std::move(callback)));
}

ukm::SourceId WebStateWrapper::GetPageUkmSourceId() {
  return web_state_ ? ukm::GetSourceIdForWebStateDocument(web_state_)
                    : ukm::kInvalidSourceId;
}

void WebStateWrapper::ClearWebStatePointer() {
  web_state_ = nullptr;
}

}  // namespace commerce
