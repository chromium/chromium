// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_IOS_IOS_RESTORE_LIVE_TAB_H_
#define COMPONENTS_SESSIONS_IOS_IOS_RESTORE_LIVE_TAB_H_

#include "components/sessions/ios/ios_live_tab.h"
#include "ios/web/public/session/proto/navigation.pb.h"

namespace web::proto {
class NavigationStorage;
}

namespace sessions {

// An implementation of LiveTab that is backed by web::CRWSessionStorage for use
// when restoring tabs from a crashed session.
class SESSIONS_EXPORT RestoreIOSLiveTab : public IOSLiveTab {
 public:
  explicit RestoreIOSLiveTab(web::proto::NavigationStorage storage);
  ~RestoreIOSLiveTab() override;
  RestoreIOSLiveTab(const RestoreIOSLiveTab&) = delete;
  RestoreIOSLiveTab& operator=(const RestoreIOSLiveTab&) = delete;

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
  const web::proto::NavigationStorage storage_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_IOS_IOS_RESTORE_LIVE_TAB_H_
