// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_

class PrefService;

namespace safe_browsing {

// Provides access to local state preferences.
class SafeBrowsingLocalStateDelegate {
 public:
  SafeBrowsingLocalStateDelegate() = default;
  virtual ~SafeBrowsingLocalStateDelegate() = default;
  SafeBrowsingLocalStateDelegate(const SafeBrowsingLocalStateDelegate&) =
      delete;
  SafeBrowsingLocalStateDelegate& operator=(
      const SafeBrowsingLocalStateDelegate&) = delete;
  // Returns the local state preference service.
  virtual PrefService* GetLocalState() = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_LOCAL_STATE_DELEGATE_H_
