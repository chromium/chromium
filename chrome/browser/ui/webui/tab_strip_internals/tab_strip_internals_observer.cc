// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_observer.h"

#include "base/check_op.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/core/tab_restore_service.h"

TabStripInternalsObserver::TabStripInternalsObserver(Profile* profile,
                                                     UpdateCallback callback)
    : callback_(std::move(callback)) {
  BrowserList::AddObserver(this);
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        StartObservingBrowser(browser);
        return true;
      });
  StartObservingTabRestore(profile);
}

TabStripInternalsObserver::~TabStripInternalsObserver() {
  BrowserList::RemoveObserver(this);
  TabStripModelObserver::StopObservingAll(this);
  StopObservingTabRestore();
}

void TabStripInternalsObserver::OnBrowserAdded(Browser* browser) {
  StartObservingBrowser(browser);
  FireUpdate();
}

void TabStripInternalsObserver::OnBrowserRemoved(Browser* browser) {
  StopObservingBrowser(browser);
  FireUpdate();
}

void TabStripInternalsObserver::OnTabStripModelChanged(
    TabStripModel* /*tab_strip_model*/,
    const TabStripModelChange& /*change*/,
    const TabStripSelectionChange& /*selection*/) {
  FireUpdate();
}

void TabStripInternalsObserver::OnTabGroupChanged(
    const TabGroupChange& /*change*/) {
  FireUpdate();
}

void TabStripInternalsObserver::OnSplitTabChanged(
    const SplitTabChange& /*change*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabChangedAt(content::WebContents* /*contents*/,
                                             int /*index*/,
                                             TabChangeType /*change_type*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabPinnedStateChanged(
    TabStripModel* /*tab_strip_model*/,
    content::WebContents* /*contents*/,
    int /*index*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabBlockedStateChanged(
    content::WebContents* /*contents*/,
    int /*index*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabGroupedStateChanged(
    TabStripModel* /*tab_strip_model*/,
    std::optional<tab_groups::TabGroupId> /*old_group*/,
    std::optional<tab_groups::TabGroupId> /*new_group*/,
    tabs::TabInterface* /*tab*/,
    int /*index*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabRestoreServiceChanged(
    sessions::TabRestoreService* /*service*/) {
  FireUpdate();
}

void TabStripInternalsObserver::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  CHECK_EQ(service, service_);
  StopObservingTabRestore();
}

// Private methods
void TabStripInternalsObserver::StartObservingBrowser(
    BrowserWindowInterface* browser) {
  if (TabStripModel* const tab_strip_model = browser->GetTabStripModel()) {
    tab_strip_model->AddObserver(this);
  }
}

void TabStripInternalsObserver::StopObservingBrowser(
    BrowserWindowInterface* browser) {
  if (TabStripModel* const tab_strip_model = browser->GetTabStripModel()) {
    tab_strip_model->RemoveObserver(this);
  }
}

void TabStripInternalsObserver::StartObservingTabRestore(Profile* profile) {
  if (profile->IsOffTheRecord()) {
    return;
  }
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (service) {
    service_ = service;
    service_->AddObserver(this);
  }
}

void TabStripInternalsObserver::StopObservingTabRestore() {
  if (service_) {
    service_->RemoveObserver(this);
    service_ = nullptr;
  }
}

void TabStripInternalsObserver::FireUpdate() {
  // TODO (crbug.com/427204855): Throttle updates by debouncing once
  // TabRestoreServiceObserver has been implemented.
  callback_.Run();
}
