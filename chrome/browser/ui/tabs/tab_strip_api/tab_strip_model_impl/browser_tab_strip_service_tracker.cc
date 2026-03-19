// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_model_impl/browser_tab_strip_service_tracker.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"

namespace tabs_api {

namespace {

TabStripService* GetTabStripService(BrowserWindowInterface* browser) {
  auto* feature = browser->GetFeatures().tab_strip_service_feature();
  return feature ? feature->GetTabStripService() : nullptr;
}

}  // namespace

BrowserTabStripServiceTracker::BrowserTabStripServiceTracker(
    Profile* profile,
    BrowserFilterCallback filter)
    : profile_(profile), filter_(std::move(filter)) {
  auto* collection = ProfileBrowserCollection::GetForProfile(profile_);
  browser_collection_observation_.Observe(collection);
}

BrowserTabStripServiceTracker::~BrowserTabStripServiceTracker() = default;

void BrowserTabStripServiceTracker::SetOnAddedCallback(
    ServiceCallback on_added) {
  service_added_callback_ = std::move(on_added);
}

void BrowserTabStripServiceTracker::SetOnRemovedCallback(
    ServiceCallback on_removed) {
  service_removed_callback_ = std::move(on_removed);
}

std::vector<TabStripService*>
BrowserTabStripServiceTracker::GetExistingServices() {
  std::vector<TabStripService*> services;

  ProfileBrowserCollection::GetForProfile(profile_)->ForEach(
      [this, &services](BrowserWindowInterface* browser) {
        if (!filter_.Run(browser)) {
          return true;
        }

        if (auto* service = GetTabStripService(browser)) {
          services.push_back(service);
        }

        return true;
      });

  return services;
}

void BrowserTabStripServiceTracker::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (!filter_.Run(browser)) {
    return;
  }

  if (auto* service = GetTabStripService(browser)) {
    if (service_added_callback_) {
      service_added_callback_.Run(service);
    }
  }
}

void BrowserTabStripServiceTracker::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (!filter_.Run(browser)) {
    return;
  }

  if (auto* service = GetTabStripService(browser)) {
    if (service_removed_callback_) {
      service_removed_callback_.Run(service);
    }
  }
}

}  // namespace tabs_api
