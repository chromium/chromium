// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"

#include "base/check_deref.h"
#include "base/notimplemented.h"

ProfileBrowserCollection::ProfileBrowserCollection(Profile* profile)
    : profile_(CHECK_DEREF(profile)) {}

ProfileBrowserCollection::~ProfileBrowserCollection() = default;

// static
ProfileBrowserCollection* ProfileBrowserCollection::GetForProfile(
    Profile* profile) {
  // TODO(crbug.com/474120522): implement
  NOTIMPLEMENTED();
  return nullptr;
}
