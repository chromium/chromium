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
#include "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

namespace {

// This is duplicated here from ios/web/js_messaging/web_view_js_utils.mm in
// order to handle numbers. The dom distiller proto expects integers and the
// generated JSON deserializer does not accept doubles in the place of ints.
// However WKWebView only returns "numbers." However, here the proto expects
// integers and doubles, which is done by checking if the number has a fraction
// or not; since this is a hacky method it's isolated to this file so as to
// limit the risk of broken JS calls.

int const kMaximumParsingRecursionDepth = 6;

// Returns a clone of |value| where double values are converted to integers if
// the numbers has no fraction. |value| is only processed up to |max_depth|.
base::Value ConvertedResultFromScriptResult(const base::Value* value,
                                            int max_depth) {
  base::Value result;
  if (!value || value->is_none()) {
    DCHECK_EQ(result.type(), base::Value::Type::NONE);
    return result;
  }

  if (max_depth < 0) {
    DLOG(WARNING) << "JS maximum recursion depth exceeded.";
    return result;
  }

  if (value->is_string()) {
    result = base::Value(value->GetString());
    DCHECK_EQ(result.type(), base::Value::Type::STRING);
  } else if (value->is_double()) {
    // Different implementation is here.
    double double_value = value->GetDouble();
    int int_value = round(double_value);
    if (double_value == int_value) {
      result = base::Value(int_value);
      DCHECK_EQ(result.type(), base::Value::Type::INTEGER);
    } else {
      result = base::Value(double_value);
      DCHECK_EQ(result.type(), base::Value::Type::DOUBLE);
    }
    // End of different implementation.
  } else if (value->is_int()) {
    result = base::Value(value->GetInt());
    DCHECK_EQ(result.type(), base::Value::Type::INTEGER);
  } else if (value->is_bool()) {
    result = base::Value(value->GetBool());
    DCHECK_EQ(result.type(), base::Value::Type::BOOLEAN);
  } else if (value->is_dict()) {
    base::Value::Dict dictionary;
    for (const auto kv : value->GetDict()) {
      base::Value item_value =
          ConvertedResultFromScriptResult(&kv.second, max_depth - 1);

      if (item_value.type() == base::Value::Type::NONE) {
        return result;
      }
      dictionary.SetByDottedPath(kv.first, std::move(item_value));
    }
    result = base::Value(std::move(dictionary));
    DCHECK_EQ(result.type(), base::Value::Type::DICT);

  } else if (value->is_list()) {
    base::Value::List list;
    for (const base::Value& list_item : value->GetList()) {
      base::Value converted_item =
          ConvertedResultFromScriptResult(&list_item, max_depth - 1);
      if (converted_item.type() == base::Value::Type::NONE) {
        return result;
      }

      list.Append(std::move(converted_item));
    }
    result = base::Value(std::move(list));
    DCHECK_EQ(result.type(), base::Value::Type::LIST);
  } else {
    NOTREACHED_IN_MIGRATION();  // Convert other types as needed.
  }
  return result;
}

}  // namespace

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

bool DistillerPageIOS::StringifyOutput() {
  return false;
}

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
    web_state_->AddObserver(this);
    media_blocker_ =
        std::make_unique<DistillerPageMediaBlocker>(web_state_.get());
  }
}

std::unique_ptr<web::WebState> DistillerPageIOS::DetachWebState() {
  if (web_state_) {
    media_blocker_.reset();
    web_state_->RemoveObserver(this);
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
  base::Value result_as_value =
      ConvertedResultFromScriptResult(result, kMaximumParsingRecursionDepth);

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
  NOTREACHED_IN_MIGRATION();
}

}  // namespace dom_distiller
