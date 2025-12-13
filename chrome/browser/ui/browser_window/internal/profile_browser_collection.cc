// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"

ProfileBrowserCollection::ProfileBrowserCollection(Profile* profile)
    : profile_(CHECK_DEREF(profile)) {}

ProfileBrowserCollection::~ProfileBrowserCollection() = default;

// static
ProfileBrowserCollection* ProfileBrowserCollection::GetForProfile(
    Profile* profile) {
  return BrowserManagerServiceFactory::GetForProfile(profile);
}
