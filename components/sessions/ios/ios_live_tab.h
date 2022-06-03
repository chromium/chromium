// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_IOS_IOS_LIVE_TAB_H_
#define COMPONENTS_SESSIONS_IOS_IOS_LIVE_TAB_H_

#include "components/sessions/core/live_tab.h"
#import "ios/web/public/web_state.h"

namespace sessions {

class SESSIONS_EXPORT IOSLiveTab : public LiveTab {
 public:
  ~IOSLiveTab() override;

  // The backing WebState of the live tab, or nullptr is it does not exist.
  virtual const web::WebState* GetWebState() const = 0;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_IOS_IOS_LIVE_TAB_H_
