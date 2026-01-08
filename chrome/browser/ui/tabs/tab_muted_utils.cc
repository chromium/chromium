// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_muted_utils.h"

#include "content/public/browser/web_contents.h"

TabMutedReason GetTabAudioMutedReason(content::WebContents* contents) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  return metadata->reason;
}

bool SetTabAudioMuted(content::WebContents* contents,
                      bool mute,
                      TabMutedReason reason,
                      const std::string& extension_id) {
  DCHECK(contents);
  DCHECK(TabMutedReason::kNone != reason);

  // Set the metadata *before* muting the contents. This ensures that any
  // observers of the WebContents that monitor mute state changes get the proper
  // value if they check.
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  metadata->reason = reason;
  if (reason == TabMutedReason::kExtension) {
    DCHECK(!extension_id.empty());
    metadata->extension_id = extension_id;
  } else {
    metadata->extension_id.clear();
  }

  contents->SetAudioMuted(mute);

  return true;
}

LastMuteMetadata::LastMuteMetadata(content::WebContents* contents)
    : content::WebContentsUserData<LastMuteMetadata>(*contents) {}
LastMuteMetadata::~LastMuteMetadata() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(LastMuteMetadata);
