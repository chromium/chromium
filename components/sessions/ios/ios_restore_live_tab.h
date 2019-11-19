// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_IOS_IOS_RESTORE_LIVE_TAB_H_
#define COMPONENTS_SESSIONS_IOS_IOS_RESTORE_LIVE_TAB_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/sessions/core/live_tab.h"

@class CRWSessionStorage;

namespace sessions {

// An implementation of LiveTab that is backed by web::CRWSessionStorage for use
// when restoring tabs from a crashed session.
class SESSIONS_EXPORT RestoreIOSLiveTab : public LiveTab {
 public:
  explicit RestoreIOSLiveTab(CRWSessionStorage* session);
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
  const std::string& GetUserAgentOverride() override;

 private:
  CRWSessionStorage* session_;

  // Needed to return an empty string in GetUserAgentOverride().
  const std::string user_agent_override_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_IOS_IOS_RESTORE_LIVE_TAB_H_
