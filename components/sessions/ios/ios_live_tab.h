// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_IOS_IOS_LIVE_TAB_H_
#define COMPONENTS_SESSIONS_IOS_IOS_LIVE_TAB_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/ios/ios_serialized_navigation_builder.h"
#import "ios/web/public/web_state.h"

namespace web {
class NavigationManager;
}

namespace sessions {

// An implementation of LiveTab that is backed by web::WebState for use
// on //ios/web-based platforms.
class SESSIONS_EXPORT IOSLiveTab : public LiveTab,
                                   public base::SupportsUserData::Data {
 public:
  ~IOSLiveTab() override;

  // Returns the IOSLiveTab associated with |web_state|, creating it if
  // it has not already been created.
  static IOSLiveTab* GetForWebState(web::WebState* web_state);

  // LiveTab:
  bool IsInitialBlankNavigation() override;
  int GetCurrentEntryIndex() override;
  int GetPendingEntryIndex() override;
  sessions::SerializedNavigationEntry GetEntryAtIndex(int index) override;
  sessions::SerializedNavigationEntry GetPendingEntry() override;
  int GetEntryCount() override;
  const std::string& GetUserAgentOverride() override;

  web::WebState* web_state() { return web_state_; }
  const web::WebState* web_state() const { return web_state_; }

 private:
  friend class base::SupportsUserData;

  explicit IOSLiveTab(web::WebState* web_state);

  web::NavigationManager* navigation_manager() {
    return web_state_->GetNavigationManager();
  }

  web::WebState* web_state_;

  // Needed to return an empty string in GetUserAgentOverride().
  std::string user_agent_override_;

  DISALLOW_COPY_AND_ASSIGN(IOSLiveTab);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_IOS_IOS_LIVE_TAB_H_
