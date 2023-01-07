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
#include "content/public/browser/web_contents_delegate.h"
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
    if (popup && popup->GetDelegate())
      popup->GetDelegate()->ActivateContents(popup.get());
  }
}

void PopunderPreventer::WillActivateWebContents(
    content::WebContents* activating_contents) {
  DCHECK_EQ(activating_contents_.get(), activating_contents);
  // If a popup is the active window, and the WebContents that is going to be
  // activated shares in the opener chain of that popup, then we suspect that
  // WebContents to be trying to create a popunder. Store the popup window so
  // that it can be re-activated once the dialog (or whatever is causing the
  // activation) is closed.
  Browser* active_browser = BrowserList::GetInstance()->GetLastActive();
  if (!active_browser || active_browser->is_type_normal())
    return;

  content::WebContents* active_popup =
      active_browser->tab_strip_model()->GetActiveWebContents();
  if (active_popup)
    AddPotentialPopunder(active_popup);
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

  WebContentsSet popup_opener_set = BuildOpenerSet(popup);
  WebContentsSet activator_opener_set =
      BuildOpenerSet(top_level_activating_contents);

  WebContentsSet common_openers = base::STLSetIntersection<WebContentsSet>(
      popup_opener_set, activator_opener_set);

  if (!common_openers.empty()) {
    // The popup is indeed related to the WebContents wanting to activate. Store
    // it, so we can focus it later.
    popups_.push_back(popup->GetWeakPtr());
  }
}
