// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_serialized_navigation_driver.h"

#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "ios/web/common/referrer_util.h"
#include "ios/web/public/navigation/referrer.h"

namespace sessions {

// static
SerializedNavigationDriver* SerializedNavigationDriver::Get() {
  return IOSSerializedNavigationDriver::GetInstance();
}

// static
IOSSerializedNavigationDriver*
IOSSerializedNavigationDriver::GetInstance() {
  return base::Singleton<
      IOSSerializedNavigationDriver,
      base::LeakySingletonTraits<IOSSerializedNavigationDriver>>::get();
}

IOSSerializedNavigationDriver::IOSSerializedNavigationDriver() {
}

IOSSerializedNavigationDriver::~IOSSerializedNavigationDriver() {
}

int IOSSerializedNavigationDriver::GetDefaultReferrerPolicy() const {
  return web::ReferrerPolicyDefault;
}

std::string
IOSSerializedNavigationDriver::GetSanitizedPageStateForPickle(
    const SerializedNavigationEntry* navigation) const {
  return std::string();
}

void IOSSerializedNavigationDriver::Sanitize(
    SerializedNavigationEntry* navigation) const {
  web::Referrer referrer(
      navigation->referrer_url_,
      static_cast<web::ReferrerPolicy>(navigation->referrer_policy_));

  if (!navigation->virtual_url_.SchemeIsHTTPOrHTTPS() ||
      !referrer.url.SchemeIsHTTPOrHTTPS()) {
    referrer.url = GURL();
  } else {
    if (referrer.policy < 0 || referrer.policy > web::ReferrerPolicyLast) {
      NOTREACHED_IN_MIGRATION();
      referrer.policy = web::ReferrerPolicyNever;
    }
    referrer.url = GURL(
        ReferrerHeaderValueForNavigation(navigation->virtual_url_, referrer));
  }

  // Reset the referrer if it has changed.
  if (navigation->referrer_url_ != referrer.url) {
    navigation->referrer_url_ = GURL();
    navigation->referrer_policy_ = GetDefaultReferrerPolicy();
  }
}

std::string IOSSerializedNavigationDriver::StripReferrerFromPageState(
      const std::string& page_state) const {
  return std::string();
}

}  // namespace sessions
