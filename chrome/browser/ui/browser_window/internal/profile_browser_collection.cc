// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "base/check_deref.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/android_profile_browser_collection_service.h"
#include "chrome/browser/ui/android/android_profile_browser_collection_service_factory.h"
#else
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

ProfileBrowserCollection::ProfileBrowserCollection(Profile* profile)
    : profile_(CHECK_DEREF(profile)) {}

ProfileBrowserCollection::~ProfileBrowserCollection() = default;

BrowserWindowInterface* ProfileBrowserCollection::FindTabbedBrowser(
    bool match_original_profiles) {
  Profile* original =
      match_original_profiles ? profile_->GetOriginalProfile() : nullptr;
  BrowserWindowInterface* match = nullptr;

  auto find = [&match, original](BrowserWindowInterface* browser) {
    if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
        browser->IsDeleteScheduled()) {
      return true;
    }
    if (original && browser->GetProfile()->GetOriginalProfile() != original) {
      return true;
    }
    match = browser;
    return false;  // stop iterating
  };

  if (match_original_profiles) {
    GlobalBrowserCollection::GetInstance()->ForEach(find, Order::kActivation);
  } else {
    ForEach(find, Order::kActivation);
  }
  return match;
}

// static
ProfileBrowserCollection* ProfileBrowserCollection::GetForProfile(
    Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  return AndroidProfileBrowserCollectionServiceFactory::GetForProfile(profile);
#else
  return BrowserManagerServiceFactory::GetForProfile(profile);
#endif  // BUILDFLAG(IS_ANDROID)
}
