// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "base/check_deref.h"
#include "build/build_config.h"

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

// static
ProfileBrowserCollection* ProfileBrowserCollection::GetForProfile(
    Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  return AndroidProfileBrowserCollectionServiceFactory::GetForProfile(profile);
#else
  return BrowserManagerServiceFactory::GetForProfile(profile);
#endif  // BUILDFLAG(IS_ANDROID)
}
