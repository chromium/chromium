// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_help_menu_utils.h"

#include <string>

#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/branded_strings.h"
#include "components/lens/lens_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/gurl.h"

namespace lens {

void FeedbackRequestedByEvent(tabs::TabInterface* tab, int event_flags) {
  chrome::ShowFeedbackPage(
      tab->GetContents()->GetLastCommittedURL(),
      tab->GetBrowserWindowInterface()->GetProfile(),
      feedback::kFeedbackSourceLensOverlay,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_LENS_SEND_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"lens_overlay",
      /*extra_diagnostics=*/std::string());
}

void InfoRequestedByEvent(tabs::TabInterface* tab, int event_flags) {
  // The tab is expected to be in the foreground.
  if (!tab->IsActivated()) {
    return;
  }
  tab->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayHelpCenterURL()),
      ui::DispositionFromEventFlags(event_flags,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void ActivityRequestedByEvent(tabs::TabInterface* tab, int event_flags) {
  // The tab is expected to be in the foreground.
  if (!tab->IsActivated()) {
    return;
  }
  tab->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayActivityURL()),
      ui::DispositionFromEventFlags(event_flags,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB));
}
}  // namespace lens
