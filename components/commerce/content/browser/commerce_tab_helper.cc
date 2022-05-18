// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/commerce_tab_helper.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace commerce {

CommerceTabHelper::CommerceTabHelper(content::WebContents* content,
                                     bool is_off_the_record,
                                     ShoppingService* shopping_service)
    : content::WebContentsObserver(content),
      content::WebContentsUserData<CommerceTabHelper>(*content),
      is_off_the_record_(is_off_the_record),
      web_wrapper_(std::make_unique<WebContentsWrapper>(content)),
      shopping_service_(shopping_service) {
  if (shopping_service_)
    shopping_service_->WebWrapperCreated(web_wrapper_.get());
}

CommerceTabHelper::~CommerceTabHelper() = default;

void CommerceTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!shopping_service_ || navigation_handle->IsSameDocument() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  shopping_service_->DidNavigatePrimaryMainFrame(web_wrapper_.get());
}

void CommerceTabHelper::WebContentsDestroyed() {
  if (shopping_service_)
    shopping_service_->WebWrapperDestroyed(web_wrapper_.get());

  web_wrapper_->ClearWebContentsPointer();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceTabHelper);

}  // namespace commerce
