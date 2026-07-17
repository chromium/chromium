// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_state.h"

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"

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

void FindBarState::SetLastSearchText(const std::u16string& text,
                                     content::WebContents* web_contents) {
  last_prepopulate_text_ = text;
  if (web_contents && web_contents->GetLastCommittedURL().is_valid()) {
    last_prepopulate_source_dte_.emplace(
        web_contents->GetLastCommittedURL(),
        ui::DataTransferEndpointOptions{
            .off_the_record =
                web_contents->GetBrowserContext()->IsOffTheRecord()});
  } else {
    last_prepopulate_source_dte_.reset();
  }
}

std::u16string FindBarState::GetSearchPrepopulateText(
    content::WebContents* web_contents) {
  std::u16string text = last_prepopulate_text_;
  std::optional<ui::DataTransferEndpoint> source_dte =
      last_prepopulate_source_dte_;

  // In incognito, if there is no previous search text, fall back to the
  // original profile's search text.
  if (text.empty() && profile_->IsOffTheRecord()) {
    FindBarState* original_state = FindBarStateFactory::GetForBrowserContext(
        profile_->GetOriginalProfile());
    text = original_state->last_prepopulate_text_;
    source_dte = original_state->last_prepopulate_source_dte_;
  }

  if (text.empty()) {
    return text;
  }

  if (!web_contents) {
    return text;
  }

  content::ClipboardEndpoint source(
      source_dte,
      base::BindRepeating(
          [](Profile* profile) -> content::BrowserContext* { return profile; },
          profile_.get()));

  content::ClipboardEndpoint destination =
      content::CreateClipboardEndpoint(*web_contents->GetPrimaryMainFrame());

  if (!enterprise_data_protection::PrepopulateFindBarTextAllowed(source,
                                                                 destination)) {
    return std::u16string();
  }

  return text;
}
