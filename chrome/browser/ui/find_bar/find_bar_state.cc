// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_state.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"

FindBarState::FindBarState(content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)) {}

FindBarState::~FindBarState() = default;

// static
void FindBarState::ConfigureWebContents(content::WebContents* web_contents) {
  find_in_page::FindTabHelper::CreateForWebContents(web_contents);
  find_in_page::FindTabHelper::FromWebContents(web_contents)
      ->set_delegate(FindBarStateFactory::GetForBrowserContext(
          web_contents->GetBrowserContext()));
}

void FindBarState::SetLastSearchText(const std::u16string& text) {
  last_prepopulate_text_ = text;
}

std::u16string FindBarState::GetSearchPrepopulateText() {
  std::u16string text = last_prepopulate_text_;

  // In incognito, if there is no previous search text, fall back to the
  // original profile's search text.
  if (text.empty() && profile_->IsOffTheRecord()) {
    text = FindBarStateFactory::GetForBrowserContext(
               profile_->GetOriginalProfile())
               ->last_prepopulate_text_;
  }

  return text;
}
