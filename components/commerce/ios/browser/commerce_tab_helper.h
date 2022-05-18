// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_IOS_BROWSER_COMMERCE_TAB_HELPER_H_
#define COMPONENTS_COMMERCE_IOS_BROWSER_COMMERCE_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/web_wrapper.h"
#include "components/commerce/ios/browser/web_state_wrapper.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

namespace commerce {

// This tab helper creates and maintains a WebWrapper that is backed by
// WebState. Events that occur on the wrapper are reported back to the
// shopping service where they are used by various commerce features.
class CommerceTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<CommerceTabHelper> {
 public:
  ~CommerceTabHelper() override;
  CommerceTabHelper(const CommerceTabHelper& other) = delete;
  CommerceTabHelper& operator=(const CommerceTabHelper& other) = delete;

  static void CreateForWebState(web::WebState* web_state,
                                bool is_off_the_record,
                                ShoppingService* shopping_service);

  // web::WebStateObserver implementation
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<CommerceTabHelper>;

  CommerceTabHelper(web::WebState* contents,
                    bool is_off_the_record,
                    ShoppingService* shopping_service);

  const bool is_off_the_record_;

  std::unique_ptr<WebStateWrapper> web_wrapper_;

  raw_ptr<ShoppingService> shopping_service_;

  // Automatically remove this observer from its host when destroyed.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      scoped_observation_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_IOS_BROWSER_COMMERCE_TAB_HELPER_H_
