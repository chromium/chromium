// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/page_info_dialog.h"

#include "base/no_destructor.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

bool ShowPageInfoDialog(content::WebContents* web_contents,
                        bubble_anchor_util::Anchor anchor) {
  if (!web_contents)
    return false;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return false;

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry)
    return false;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  ShowPageInfoDialogImpl(browser, web_contents, entry->GetVirtualURL(),
                         security_info, anchor);

  if (GetPageInfoDialogCreatedCallbackForTesting())
    std::move(GetPageInfoDialogCreatedCallbackForTesting()).Run();

  return true;
}

base::OnceClosure& GetPageInfoDialogCreatedCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> closure;
  return *closure;
}
