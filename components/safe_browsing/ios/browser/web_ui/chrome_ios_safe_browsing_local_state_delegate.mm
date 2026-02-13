// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/web_ui/chrome_ios_safe_browsing_local_state_delegate.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace safe_browsing {

PrefService* ChromeIOSSafeBrowsingLocalStateDelegate::GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

}  // namespace safe_browsing
