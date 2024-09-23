// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popunder_preventer.h"

#include <set>

#include "base/stl_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#endif

namespace {

using WebContentsSet = std::set<content::WebContents*>;

WebContentsSet BuildOpenerSet(content::WebContents* contents) {
  WebContentsSet result;
  while (contents) {
    result.insert(contents);
    contents = contents->GetFirstWebContentsInLiveOriginalOpenerChain();
  }

  return result;
}

}  // namespace

PopunderPreventer::PopunderPreventer(content::WebContents* activating_contents)
    : activating_contents_(activating_contents->GetWeakPtr()) {
  WillActivateWebContents(activating_contents);
}

PopunderPreventer::~PopunderPreventer() {
  for (base::WeakPtr<content::WebContents>& popup : popups_) {
    auto* browser = popup ? chrome::FindBrowserWithTab(popup.get()) : nullptr;
    // Only popup, app, or app-popup browser windows are potential popunders.
    if (browser && (browser->is_type_app() || browser->is_type_popup() ||
                    browser->is_type_app_popup())) {
      browser->ActivateContents(popup.get());
    }
  }
}

void PopunderPreventer::WillActivateWebContents(
    content::WebContents* activating_contents) {
  DCHECK_EQ(activating_contents_.get(), activating_contents);
  // Check if the active window may be a popunder of `activating_contents`.
  if (Browser* active = BrowserList::GetInstance()->GetLastActive()) {
    AddPotentialPopunder(active->tab_strip_model()->GetActiveWebContents());
  }
}

void PopunderPreventer::AddPotentialPopunder(content::WebContents* popup) {
  content::WebContents* top_level_activating_contents =
      activating_contents_.get();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the dialog was triggered via an PDF, get the top level web contents that
  // embeds the PDF.
  top_level_activating_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(
          top_level_activating_contents);
#endif

  // Check if `popup` and `activating_contents_` share an opener chain entry.
  // Ignore `activating_contents_` itself; reactivating that on destruction
  // causes rentrancy, e.g. when exiting app fullscreen: crbug.com/331095620
  WebContentsSet common_openers = base::STLSetIntersection<WebContentsSet>(
      BuildOpenerSet(popup), BuildOpenerSet(top_level_activating_contents));
  if (popup != top_level_activating_contents && !common_openers.empty()) {
    // Store the suspected popunder, which should be focused later.
    popups_.push_back(popup->GetWeakPtr());
  }
}
