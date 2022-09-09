// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

// Stores per-profile state needed for find in page.  This includes the most
// recently searched for term.
class FindBarState : public KeyedService,
                     public find_in_page::FindTabHelper::Delegate {
 public:
  explicit FindBarState(content::BrowserContext* browser_context);

  FindBarState(const FindBarState&) = delete;
  FindBarState& operator=(const FindBarState&) = delete;

  ~FindBarState() override;

  // Creates a find_in_page::FindTabHelper for the given contents and sets the
  // appropriate FindBarState object as its delegate.
  static void ConfigureWebContents(content::WebContents* web_contents);

  // find_in_page::FindTabHelper::Delegate:
  void SetLastSearchText(const std::u16string& text) override;
  std::u16string GetSearchPrepopulateText() override;

 private:
  raw_ptr<Profile> profile_;
  std::u16string last_prepopulate_text_;
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_H_
