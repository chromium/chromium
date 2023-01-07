// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"

namespace send_tab_to_self {

// These methods should only be thin wrappers around
// SendTabToSelfBubbleController that can be called outside of /views/.

void ShowBubble(content::WebContents* web_contents, bool show_back_button) {
  return SendTabToSelfBubbleController::CreateOrGetFromWebContents(web_contents)
      ->ShowBubble(show_back_button);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* user_prefs) {
  SendTabToSelfBubbleController::RegisterProfilePrefs(user_prefs);
}

}  // namespace send_tab_to_self
