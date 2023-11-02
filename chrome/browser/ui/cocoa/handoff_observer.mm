// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/handoff_observer.h"

#include "base/check.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

HandoffObserver::HandoffObserver(NSObject<HandoffObserverDelegate>* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  BrowserList::AddObserver(this);
  SetActiveBrowser(chrome::FindLastActive());
}

HandoffObserver::~HandoffObserver() {
  BrowserList::RemoveObserver(this);
  SetActiveBrowser(nullptr);
}

void HandoffObserver::OnBrowserSetLastActive(Browser* browser) {
  SetActiveBrowser(browser);
  [delegate_ handoffContentsChanged:GetActiveWebContents()];
}

void HandoffObserver::OnBrowserRemoved(Browser* removed_browser) {
  if (active_browser_ != removed_browser)
    return;

  SetActiveBrowser(chrome::FindLastActive());
  [delegate_ handoffContentsChanged:GetActiveWebContents()];
}

void HandoffObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  StartObservingWebContents(selection.new_contents);
  [delegate_ handoffContentsChanged:GetActiveWebContents()];
}

void HandoffObserver::PrimaryPageChanged(content::Page& page) {
  [delegate_ handoffContentsChanged:GetActiveWebContents()];
}

void HandoffObserver::TitleWasSet(content::NavigationEntry* entry) {
  [delegate_ handoffContentsChanged:GetActiveWebContents()];
}

void HandoffObserver::SetActiveBrowser(Browser* active_browser) {
  if (active_browser == active_browser_)
    return;

  if (active_browser_) {
    active_browser_->tab_strip_model()->RemoveObserver(this);
    StopObservingWebContents();
  }

  active_browser_ = active_browser;

  if (active_browser_) {
    active_browser_->tab_strip_model()->AddObserver(this);
    content::WebContents* web_contents = GetActiveWebContents();
    if (web_contents)
      StartObservingWebContents(web_contents);
  }
}

void HandoffObserver::StartObservingWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Observe(web_contents);
}

void HandoffObserver::StopObservingWebContents() {
  Observe(nullptr);
}

content::WebContents* HandoffObserver::GetActiveWebContents() {
  if (!active_browser_)
    return nullptr;

  return active_browser_->tab_strip_model()->GetActiveWebContents();
}
