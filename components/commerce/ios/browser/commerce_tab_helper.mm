// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/ios/browser/commerce_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/web_state.h"
#include "net/http/http_response_headers.h"

namespace commerce {

namespace {
bool ShouldUpdateRecentlyViewedURL(web::WebState* web_state,
                                   web::NavigationContext* navigation_context) {
  if (navigation_context->GetError()) {
    return false;
  }

  if (navigation_context->GetResponseHeaders() &&
      navigation_context->GetResponseHeaders()->response_code() == 404) {
    return false;
  }

  if (!navigation_context->HasCommitted() ||
      !web_state->GetNavigationManager()->GetLastCommittedItem()) {
    // Navigation was replaced or aborted.
    return false;
  }

  web::NavigationItem* last_committed_item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  const GURL& url = last_committed_item->GetURL();
  if (url.SchemeIs(url::kDataScheme)) {
    return false;
  }

  return true;
}
}  // namespace

CommerceTabHelper::CommerceTabHelper(web::WebState* state,
                                     bool is_off_the_record,
                                     ShoppingService* shopping_service)
    : is_off_the_record_(is_off_the_record),
      web_wrapper_(std::make_unique<WebStateWrapper>(state)),
      shopping_service_(shopping_service) {
  scoped_observation_.Observe(state);

  if (shopping_service_)
    shopping_service_->WebWrapperCreated(web_wrapper_.get());
}

CommerceTabHelper::~CommerceTabHelper() = default;

void CommerceTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!shopping_service_ ||
      previous_main_frame_url_ == navigation_context->GetUrl()) {
    return;
  }

  // Notify the service that we're no longer interested in a particular URL.
  shopping_service_->DidNavigateAway(web_wrapper_.get(),
                                     previous_main_frame_url_);
  previous_main_frame_url_ = navigation_context->GetUrl();

  // If navigating to a different document, expect PageLoaded to be
  // triggered.
  if (!navigation_context->IsSameDocument()) {
    web_wrapper_->SetIsFirstLoadForNavigationFinished(false);
  }

  shopping_service_->DidNavigatePrimaryMainFrame(web_wrapper_.get());

  if (web_state->IsVisible() &&
      ShouldUpdateRecentlyViewedURL(web_state, navigation_context)) {
    shopping_service_->OnWebWrapperViewed(web_wrapper_.get());
  }
}

void CommerceTabHelper::DidStopLoading(web::WebState* web_state) {
  if (!shopping_service_) {
    return;
  }

  web_wrapper_->SetIsFirstLoadForNavigationFinished(true);
  shopping_service_->DidStopLoading(web_wrapper_.get());
}

void CommerceTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (!shopping_service_)
    return;

  web_wrapper_->SetIsFirstLoadForNavigationFinished(true);

  shopping_service_->DidFinishLoad(web_wrapper_.get());
}

void CommerceTabHelper::WasShown(web::WebState* web_state) {
  if (!shopping_service_) {
    return;
  }

  shopping_service_->OnWebWrapperSwitched(web_wrapper_.get());
}

void CommerceTabHelper::WebStateDestroyed(web::WebState* web_state) {
  if (shopping_service_)
    shopping_service_->WebWrapperDestroyed(web_wrapper_.get());

  web_wrapper_->ClearWebStatePointer();

  // This needs to be reset prior to the destruction of the web state in order
  // to prevent a CHECK failure for observer count.
  scoped_observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(CommerceTabHelper)

}  // namespace commerce
