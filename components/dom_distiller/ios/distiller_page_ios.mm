// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/ios/distiller_page_ios.h"

#import <UIKit/UIKit.h>

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/deprecated/crw_js_injection_manager.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// This is duplicated here from ios/web/js_messaging/web_view_js_utils.mm in
// order to handle numbers. The dom distiller proto expects integers and the
// generated JSON deserializer does not accept doubles in the place of ints.
// However WKWebView only returns "numbers." However, here the proto expects
// integers and doubles, which is done by checking if the number has a fraction
// or not; since this is a hacky method it's isolated to this file so as to
// limit the risk of broken JS calls.

int const kMaximumParsingRecursionDepth = 6;

// Converts result of WKWebView script evaluation to base::Value, parsing
// |wk_result| up to a depth of |max_depth|.
base::Value ValueResultFromScriptResult(id wk_result, int max_depth) {
  base::Value result;
  if (!wk_result) {
    return result;
  }

  if (max_depth < 0) {
    DLOG(WARNING) << "JS maximum recursion depth exceeded.";
    return result;
  }

  CFTypeID result_type = CFGetTypeID(reinterpret_cast<CFTypeRef>(wk_result));
  if (result_type == CFStringGetTypeID()) {
    result = base::Value(base::SysNSStringToUTF8(wk_result));
    DCHECK_EQ(result.type(), base::Value::Type::STRING);
  } else if (result_type == CFNumberGetTypeID()) {
    // Different implementation is here.
    if ([wk_result intValue] != [wk_result doubleValue]) {
      result = base::Value([wk_result doubleValue]);
      DCHECK_EQ(result.type(), base::Value::Type::DOUBLE);
    } else {
      result = base::Value([wk_result intValue]);
      DCHECK_EQ(result.type(), base::Value::Type::INTEGER);
    }
    // End of different implementation.
  } else if (result_type == CFBooleanGetTypeID()) {
    result = base::Value(static_cast<bool>([wk_result boolValue]));
    DCHECK_EQ(result.type(), base::Value::Type::BOOLEAN);
  } else if (result_type == CFNullGetTypeID()) {
    DCHECK_EQ(result.type(), base::Value::Type::NONE);
  } else if (result_type == CFDictionaryGetTypeID()) {
    base::Value dictionary(base::Value::Type::DICTIONARY);
    for (id key in wk_result) {
      NSString* obj_c_string = base::mac::ObjCCast<NSString>(key);
      base::Value value =
          ValueResultFromScriptResult(wk_result[obj_c_string], max_depth - 1);

      if (value.type() == base::Value::Type::NONE) {
        return result;
      }

      std::vector<base::StringPiece> path =
          base::SplitStringPiece(base::SysNSStringToUTF8(obj_c_string), ".",
                                 base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      dictionary.SetPath(path, std::move(value));
    }
    result = std::move(dictionary);
    DCHECK_EQ(result.type(), base::Value::Type::DICTIONARY);
  } else if (result_type == CFArrayGetTypeID()) {
    std::vector<base::Value> list;
    for (id list_item in wk_result) {
      base::Value value = ValueResultFromScriptResult(list_item, max_depth - 1);
      if (value.type() == base::Value::Type::NONE) {
        return result;
      }

      list.push_back(std::move(value));
    }
    result = base::Value(list);
    DCHECK_EQ(result.type(), base::Value::Type::LIST);
  } else {
    NOTREACHED();  // Convert other types as needed.
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

  bool ShouldAllowResponse(NSURLResponse* response,
                           bool for_main_frame) override {
    if ([response.MIMEType hasPrefix:@"audio/"] ||
        [response.MIMEType hasPrefix:@"video/"]) {
      if (for_main_frame) {
        main_frame_navigation_blocked_ = true;
      }
      return NO;
    }
    return YES;
  }

  bool main_frame_navigation_blocked() const {
    return main_frame_navigation_blocked_;
  }

 private:
  bool main_frame_navigation_blocked_;
  DISALLOW_COPY_AND_ASSIGN(DistillerPageMediaBlocker);
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
  // Load page using WebState.
  web::NavigationManager::WebLoadParams params(url_);
  web_state_->SetKeepRenderProcessAlive(true);
  web_state_->GetNavigationManager()->LoadURLWithParams(params);
  // LoadIfNecessary is needed because the view is not created (but needed) when
  // loading the page. TODO(crbug.com/705819): Remove this call.
  web_state_->GetNavigationManager()->LoadIfNecessary();
}

void DistillerPageIOS::OnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  // Don't attempt to distill if the page load failed or if there is no
  // WebState.
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE ||
      !web_state_) {
    HandleJavaScriptResult(nil);
    return;
  }
  // Inject the script.
  base::WeakPtr<DistillerPageIOS> weak_this = weak_ptr_factory_.GetWeakPtr();
  [[web_state_->GetJSInjectionReceiver()
      instanceOfClass:[CRWJSInjectionManager class]]
      executeJavaScript:base::SysUTF8ToNSString(script_)
      completionHandler:^(id result, NSError* error) {
        DistillerPageIOS* distiller_page = weak_this.get();
        if (distiller_page)
          distiller_page->HandleJavaScriptResult(result);
      }];
}

void DistillerPageIOS::HandleJavaScriptResult(id result) {
  base::Value result_as_value =
      ValueResultFromScriptResult(result, kMaximumParsingRecursionDepth);

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
  if (web_state->IsShowingWebInterstitial() ||
      media_blocker_->main_frame_navigation_blocked()) {
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
