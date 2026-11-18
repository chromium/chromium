// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_ASYNC_WEB_CONTENTS_DESTROYER_DATA_H_
#define COMPONENTS_TABS_PUBLIC_ASYNC_WEB_CONTENTS_DESTROYER_DATA_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {

// Attached to a WebContents when it is about to be asynchronously destroyed
// (e.g., by TabWebContentsDestroyer) to signal that some actions should no
// longer be taken.
class AsyncWebContentsDestroyerData
    : public content::WebContentsUserData<AsyncWebContentsDestroyerData> {
 public:
  AsyncWebContentsDestroyerData(const AsyncWebContentsDestroyerData&) = delete;
  AsyncWebContentsDestroyerData& operator=(
      const AsyncWebContentsDestroyerData&) = delete;

  ~AsyncWebContentsDestroyerData() override;

  // Returns true if `web_contents` is non-null and marked as about to be
  // asynchronously destroyed.
  static bool WillBeDestroyed(const content::WebContents* web_contents);

 private:
  explicit AsyncWebContentsDestroyerData(content::WebContents* web_contents);

  friend class content::WebContentsUserData<AsyncWebContentsDestroyerData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_ASYNC_WEB_CONTENTS_DESTROYER_DATA_H_
