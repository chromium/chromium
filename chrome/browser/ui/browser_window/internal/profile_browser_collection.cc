// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"

// static
ProfileBrowserCollection* ProfileBrowserCollection::GetForProfile(
    Profile* profile) {
  return BrowserManagerServiceFactory::GetForProfile(profile);
}
