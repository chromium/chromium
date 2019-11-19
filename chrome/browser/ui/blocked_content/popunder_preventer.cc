// Copyright 2014 The Chromium Authors. All rights reserved.
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
    contents = content::WebContents::FromRenderFrameHost(
        contents->GetOriginalOpener());
  }

  return result;
}

}  // namespace

PopunderPreventer::PopunderPreventer(content::WebContents* activating_contents)
    : popup_(nullptr) {
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
  if (!active_popup)
    return;

  content::WebContents* actual_activating_contents = activating_contents;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the dialog was triggered via an PDF, get the actual web contents that
  // embeds the PDF.
  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromWebContents(activating_contents);
  if (guest)
    actual_activating_contents = guest->embedder_web_contents();
#endif

  WebContentsSet popup_opener_set = BuildOpenerSet(active_popup);
  WebContentsSet activator_opener_set =
      BuildOpenerSet(actual_activating_contents);

  WebContentsSet common_openers = base::STLSetIntersection<WebContentsSet>(
      popup_opener_set, activator_opener_set);

  if (!common_openers.empty()) {
    // The popup is indeed related to the WebContents wanting to activate. Store
    // it, so we can focus it later.
    popup_ = active_popup;
    Observe(popup_);
  }
}

PopunderPreventer::~PopunderPreventer() {
  if (!popup_)
    return;

  content::WebContentsDelegate* delegate = popup_->GetDelegate();
  if (!delegate)
    return;

  delegate->ActivateContents(popup_);
}

void PopunderPreventer::WebContentsDestroyed() {
  popup_ = nullptr;
}
