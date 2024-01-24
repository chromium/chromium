// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_IOS_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_IOS_H_

#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/core/browser/password_protection/request_canceler.h"
#include "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
}

namespace safe_browsing {

class RequestCancelerIOS : public RequestCanceler,
                           public web::WebStateObserver {
 public:
  RequestCancelerIOS(base::WeakPtr<CancelableRequest> request,
                     web::WebState* web_state);
  ~RequestCancelerIOS() override;

 private:
  // WebStateObserver implementation
  void WebStateDestroyed(web::WebState* web_state) override;

  raw_ptr<web::WebState> web_state_ = nullptr;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_PASSWORD_PROTECTION_REQUEST_CANCELER_IOS_H_
