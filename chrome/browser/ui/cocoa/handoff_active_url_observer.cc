// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/handoff_active_url_observer.h"

#include "base/check.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/handoff_active_url_observer_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

HandoffActiveURLObserver::HandoffActiveURLObserver(
    HandoffActiveURLObserverDelegate* delegate)
    : delegate_(delegate),
      active_browser_(nullptr) {
  DCHECK(delegate_);

  BrowserList::AddObserver(this);
  SetActiveBrowser(chrome::FindLastActive());
}

HandoffActiveURLObserver::~HandoffActiveURLObserver() {
  BrowserList::RemoveObserver(this);
  SetActiveBrowser(nullptr);
}

void HandoffActiveURLObserver::OnBrowserSetLastActive(Browser* browser) {
  SetActiveBrowser(browser);
  delegate_->HandoffActiveURLChanged(GetActiveWebContents());
}

void HandoffActiveURLObserver::OnBrowserRemoved(Browser* removed_browser) {
  if (active_browser_ != removed_browser)
    return;

  SetActiveBrowser(chrome::FindLastActive());
  delegate_->HandoffActiveURLChanged(GetActiveWebContents());
}

void HandoffActiveURLObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  StartObservingWebContents(selection.new_contents);
  delegate_->HandoffActiveURLChanged(selection.new_contents);
}

void HandoffActiveURLObserver::PrimaryPageChanged(content::Page& page) {
  delegate_->HandoffActiveURLChanged(web_contents());
}

void HandoffActiveURLObserver::SetActiveBrowser(Browser* active_browser) {
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

void HandoffActiveURLObserver::StartObservingWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  Observe(web_contents);
}

void HandoffActiveURLObserver::StopObservingWebContents() {
  Observe(nullptr);
}

content::WebContents* HandoffActiveURLObserver::GetActiveWebContents() {
  if (!active_browser_)
    return nullptr;

  return active_browser_->tab_strip_model()->GetActiveWebContents();
}
