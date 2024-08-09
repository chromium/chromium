// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/commerce_tab_helper.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace commerce {

CommerceTabHelper::CommerceTabHelper(content::WebContents* content,
                                     bool is_off_the_record,
                                     ShoppingService* shopping_service,
                                     int32_t js_world_id)
    : content::WebContentsObserver(content),
      content::WebContentsUserData<CommerceTabHelper>(*content),
      is_off_the_record_(is_off_the_record),
      web_wrapper_(std::make_unique<WebContentsWrapper>(content, js_world_id)),
      shopping_service_(shopping_service) {
  if (shopping_service_)
    shopping_service_->WebWrapperCreated(web_wrapper_.get());
}

CommerceTabHelper::~CommerceTabHelper() = default;

void CommerceTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!shopping_service_ || !navigation_handle->IsInPrimaryMainFrame() ||
      previous_main_frame_url_ == navigation_handle->GetURL()) {
    return;
  }

  // Notify the service that we're no longer interested in a particular URL.
  shopping_service_->DidNavigateAway(web_wrapper_.get(),
                                     previous_main_frame_url_);
  previous_main_frame_url_ = navigation_handle->GetURL();

  // If navigating to a different document, expect DidStopLoading to be
  // triggered.
  if (!navigation_handle->IsSameDocument()) {
    web_wrapper_->SetIsFirstLoadForNavigationFinished(false);
  }

  shopping_service_->DidNavigatePrimaryMainFrame(web_wrapper_.get());

  // Some single-page webapps don't trigger load events for every navigation.
  // Make sure we have the opportunity to observer that event.
  if (web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    shopping_service_->DidFinishLoad(web_wrapper_.get());
  }

  if (navigation_handle->HasCommitted() &&
      navigation_handle->ShouldUpdateHistory() &&
      web_contents()->GetFocusedFrame()) {
    shopping_service_->OnWebWrapperViewed(web_wrapper_.get());
  }
}

void CommerceTabHelper::DidStopLoading() {
  if (!shopping_service_) {
    return;
  }

  web_wrapper_->SetIsFirstLoadForNavigationFinished(true);
  shopping_service_->DidStopLoading(web_wrapper_.get());
}

void CommerceTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!shopping_service_ || !render_frame_host->IsInPrimaryMainFrame())
    return;

  web_wrapper_->SetIsFirstLoadForNavigationFinished(true);

  shopping_service_->DidFinishLoad(web_wrapper_.get());
}

void CommerceTabHelper::OnWebContentsFocused(content::RenderWidgetHost* host) {
  if (!shopping_service_) {
    return;
  }

  shopping_service_->OnWebWrapperSwitched(web_wrapper_.get());
}

void CommerceTabHelper::WebContentsDestroyed() {
  if (shopping_service_)
    shopping_service_->WebWrapperDestroyed(web_wrapper_.get());

  web_wrapper_->ClearWebContentsPointer();
}

void CommerceTabHelper::SetShoppingServiceForTesting(KeyedService* service) {
  auto* shopping_service = static_cast<commerce::ShoppingService*>(service);

  shopping_service_ = shopping_service;

  if (shopping_service_) {
    shopping_service_->WebWrapperCreated(web_wrapper_.get());
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceTabHelper);

}  // namespace commerce
