// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MUTED_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_MUTED_UTILS_H_

#include <string>

#include "chrome/browser/ui/tabs/tab_enums.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

struct LastMuteMetadata
    : public content::WebContentsUserData<LastMuteMetadata> {
  ~LastMuteMetadata() override;

  TabMutedReason reason = TabMutedReason::kNone;
  std::string extension_id;  // Only valid when |reason| is EXTENSION.

 private:
  explicit LastMuteMetadata(content::WebContents* contents);
  friend class content::WebContentsUserData<LastMuteMetadata>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Sets whether all audio output from |contents| is muted, along with the
// |reason| it is to be muted/unmuted (via UI or extension API).  When |reason|
// is TAB_MUTED_REASON_EXTENSION, |extension_id| must be provided; otherwise, it
// is ignored.  Returns whether the tab was actually muted.
bool SetTabAudioMuted(content::WebContents* contents,
                      bool mute,
                      TabMutedReason reason,
                      const std::string& extension_id);

// Returns the last reason a tab's mute state was changed.
TabMutedReason GetTabAudioMutedReason(content::WebContents* contents);

#endif  // CHROME_BROWSER_UI_TABS_TAB_MUTED_UTILS_H_
