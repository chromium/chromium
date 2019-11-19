// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/security_state/ios/insecure_input_tab_helper.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/logging.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/form_activity_tab_helper.h"
#include "components/security_state/ios/ssl_status_input_event_data.h"
#import "ios/web/common/origin_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Creates or retrieves the |user_data| object in the SSLStatus attached to the
// WebState's NavigationItem.
security_state::SSLStatusInputEventData* GetOrCreateSSLStatusInputEventData(
    web::WebState* web_state) {
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();

  // We aren't guaranteed to always have a navigation item.
  if (!item)
    return nullptr;

  web::SSLStatus& ssl = item->GetSSL();
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          ssl.user_data.get());
  if (!input_events) {
    ssl.user_data = std::make_unique<security_state::SSLStatusInputEventData>();
    input_events = static_cast<security_state::SSLStatusInputEventData*>(
        ssl.user_data.get());
  }
  return input_events;
}

}  // namespace

InsecureInputTabHelper::~InsecureInputTabHelper() = default;

// static
InsecureInputTabHelper* InsecureInputTabHelper::GetOrCreateForWebState(
    web::WebState* web_state) {
  InsecureInputTabHelper* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
    DCHECK(helper);
  }
  return helper;
}

void InsecureInputTabHelper::DidEditFieldInInsecureContext() {
  DCHECK(!web::IsOriginSecure(web_state_->GetLastCommittedURL()));

  security_state::SSLStatusInputEventData* input_events =
      GetOrCreateSSLStatusInputEventData(web_state_);
  if (!input_events)
    return;

  // If the first field edit in the web contents was just performed,
  // update the SSLStatusInputEventData.
  if (!input_events->input_events()->insecure_field_edited) {
    input_events->input_events()->insecure_field_edited = true;
    web_state_->DidChangeVisibleSecurityState();
  }
}

InsecureInputTabHelper::InsecureInputTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
  autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state)
      ->AddObserver(this);
}

void InsecureInputTabHelper::FormActivityRegistered(
    web::WebState* web_state,
    web::WebFrame* sender_frame,
    const autofill::FormActivityParams& params) {
  DCHECK_EQ(web_state_, web_state);
  if (params.type == "input" &&
      !web::IsOriginSecure(web_state->GetLastCommittedURL())) {
    DidEditFieldInInsecureContext();
  }
}

void InsecureInputTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  // Check if the navigation should clear insecure input event data (i.e., not a
  // same-document navigation).
  if (!web::IsOriginSecure(web_state->GetLastCommittedURL()) &&
      navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument()) {
    security_state::SSLStatusInputEventData* input_events =
        GetOrCreateSSLStatusInputEventData(web_state_);
    if (!input_events)
      return;
    input_events->input_events()->insecure_field_edited = false;
    web_state_->DidChangeVisibleSecurityState();
  }
}

void InsecureInputTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state)
      ->RemoveObserver(this);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(InsecureInputTabHelper)
