// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/ios/distiller_page_ios.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "components/dom_distiller/ios/distiller_page_utils.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

namespace dom_distiller {

// Blocks the media content to avoid starting background playing.
class DistillerPageMediaBlocker : public web::WebStatePolicyDecider {
 public:
  DistillerPageMediaBlocker(web::WebState* web_state)
      : web::WebStatePolicyDecider(web_state),
        main_frame_navigation_blocked_(false) {}

  DistillerPageMediaBlocker(const DistillerPageMediaBlocker&) = delete;
  DistillerPageMediaBlocker& operator=(const DistillerPageMediaBlocker&) =
      delete;

  void ShouldAllowResponse(
      NSURLResponse* response,
      web::WebStatePolicyDecider::ResponseInfo response_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override {
    if ([response.MIMEType hasPrefix:@"audio/"] ||
        [response.MIMEType hasPrefix:@"video/"]) {
      if (response_info.for_main_frame) {
        main_frame_navigation_blocked_ = true;
      }
      std::move(callback).Run(PolicyDecision::Cancel());
      return;
    }
    std::move(callback).Run(PolicyDecision::Allow());
  }

  bool main_frame_navigation_blocked() const {
    return main_frame_navigation_blocked_;
  }

 private:
  bool main_frame_navigation_blocked_;
};

#pragma mark -

DistillerPageIOS::DistillerPageIOS(web::BrowserState* browser_state)
    : browser_state_(browser_state), weak_ptr_factory_(this) {}

DistillerPageIOS::~DistillerPageIOS() {
  DetachWebState();
}

void DistillerPageIOS::AttachWebState(
    std::unique_ptr<web::WebState> web_state) {
  if (web_state_) {
    DetachWebState();
  }
  web_state_ = std::move(web_state);
  if (web_state_) {
    web_state_observation_.Observe(web_state_.get());
    media_blocker_ =
        std::make_unique<DistillerPageMediaBlocker>(web_state_.get());
  }
}

std::unique_ptr<web::WebState> DistillerPageIOS::DetachWebState() {
  if (web_state_) {
    media_blocker_.reset();
    web_state_observation_.Reset();
  }
  return std::move(web_state_);
}

web::WebState* DistillerPageIOS::CurrentWebState() {
  return web_state_.get();
}

void DistillerPageIOS::DistillPageImpl(const GURL& url,
                                       const std::string& script) {
  if (!url.is_valid() || !script.length())
    return;
  url_ = url;
  script_ = script;

  if (!web_state_) {
    const web::WebState::CreateParams web_state_create_params(browser_state_);
    std::unique_ptr<web::WebState> web_state_unique =
        web::WebState::Create(web_state_create_params);
    AttachWebState(std::move(web_state_unique));
  }

  distilling_navigation_ = true;
  // Load page using WebState.
  web::NavigationManager::WebLoadParams params(url_);
  web_state_->SetKeepRenderProcessAlive(true);
  web_state_->GetNavigationManager()->LoadURLWithParams(params);
  // LoadIfNecessary is needed because the view is not created (but needed) when
  // loading the page. TODO(crbug.com/41309809): Remove this call.
  web_state_->GetNavigationManager()->LoadIfNecessary();
}

void DistillerPageIOS::OnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  if (!distilling_navigation_) {
    // This is a second navigation after the distillation request.
    // Distillation was already requested, so ignore this one.
    return;
  }
  distilling_navigation_ = false;
  // Don't attempt to distill if the page load failed or if there is no
  // WebState.
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE ||
      !web_state_) {
    HandleJavaScriptResult(nil);
    return;
  }

  web::WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!main_frame) {
    HandleJavaScriptResult(nil);
    return;
  }

  // Inject the script.
  base::WeakPtr<DistillerPageIOS> weak_this = weak_ptr_factory_.GetWeakPtr();
  main_frame->ExecuteJavaScript(
      base::UTF8ToUTF16(script_),
      base::BindOnce(&DistillerPageIOS::HandleJavaScriptResult, weak_this));
}

void DistillerPageIOS::HandleJavaScriptResult(const base::Value* result) {
  base::Value result_as_value = ParseValueFromScriptResult(result);
  OnDistillationDone(url_, &result_as_value);
}

void DistillerPageIOS::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_.get(), web_state);
  if (!loading_) {
    return;
  }

  loading_ = false;
  OnLoadURLDone(load_completion_status);
}

void DistillerPageIOS::DidStartLoading(web::WebState* web_state) {
  DCHECK_EQ(web_state_.get(), web_state);
  loading_ = true;
}

void DistillerPageIOS::DidStopLoading(web::WebState* web_state) {
  DCHECK_EQ(web_state_.get(), web_state);
  if (media_blocker_->main_frame_navigation_blocked()) {
    // If there is an interstitial, stop the distillation.
    // The interstitial is not displayed to the user who cannot choose to
    // continue.
    PageLoaded(web_state, web::PageLoadCompletionStatus::FAILURE);
  }
}

void DistillerPageIOS::WebStateDestroyed(web::WebState* web_state) {
  // The DistillerPageIOS owns the WebState that it observe and unregister
  // itself from the WebState before destroying it, so this method should
  // never be called.
  NOTREACHED();
}

}  // namespace dom_distiller
