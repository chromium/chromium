// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_IOS_IOS_WEBSTATE_LIVE_TAB_H_
#define COMPONENTS_SESSIONS_IOS_IOS_WEBSTATE_LIVE_TAB_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/sessions/ios/ios_live_tab.h"
#include "components/sessions/ios/ios_serialized_navigation_builder.h"
#import "ios/web/public/web_state.h"

namespace web {
class NavigationManager;
}

namespace sessions {

// An implementation of LiveTab that is backed by web::WebState for use
// on //ios/web-based platforms.
class SESSIONS_EXPORT IOSWebStateLiveTab : public IOSLiveTab,
                                           public base::SupportsUserData::Data {
 public:
  IOSWebStateLiveTab(const IOSWebStateLiveTab&) = delete;
  IOSWebStateLiveTab& operator=(const IOSWebStateLiveTab&) = delete;

  ~IOSWebStateLiveTab() override;

  // Returns the IOSLiveTab associated with |web_state|, creating it if
  // it has not already been created.
  static IOSWebStateLiveTab* GetForWebState(web::WebState* web_state);

  // LiveTab:
  bool IsInitialBlankNavigation() override;
  int GetCurrentEntryIndex() override;
  int GetPendingEntryIndex() override;
  sessions::SerializedNavigationEntry GetEntryAtIndex(int index) override;
  sessions::SerializedNavigationEntry GetPendingEntry() override;
  int GetEntryCount() override;
  sessions::SerializedUserAgentOverride GetUserAgentOverride() override;

  const web::WebState* GetWebState() const override;

 private:
  friend class base::SupportsUserData;

  explicit IOSWebStateLiveTab(web::WebState* web_state);

  web::NavigationManager* navigation_manager() {
    return web_state_->GetNavigationManager();
  }

  raw_ptr<web::WebState> web_state_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_IOS_IOS_WEBSTATE_LIVE_TAB_H_
