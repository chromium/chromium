// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_CHROME_IOS_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_CHROME_IOS_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_

#import "components/safe_browsing/core/browser/web_ui/safe_browsing_local_state_delegate.h"

namespace safe_browsing {

class ChromeIOSSafeBrowsingLocalStateDelegate
    : public SafeBrowsingLocalStateDelegate {
 public:
  ChromeIOSSafeBrowsingLocalStateDelegate() = default;
  ~ChromeIOSSafeBrowsingLocalStateDelegate() override = default;
  PrefService* GetLocalState() override;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_CHROME_IOS_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_
