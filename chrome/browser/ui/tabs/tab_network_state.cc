// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_network_state.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

TabNetworkState TabNetworkStateForWebContents(content::WebContents* contents) {
  DCHECK(contents);

  if (!contents->ShouldShowLoadingUI()) {
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    if (entry && (entry->GetPageType() == content::PAGE_TYPE_ERROR))
      return TabNetworkState::kError;
    return TabNetworkState::kNone;
  }

  if (contents->IsWaitingForResponse())
    return TabNetworkState::kWaiting;
  return TabNetworkState::kLoading;
}
