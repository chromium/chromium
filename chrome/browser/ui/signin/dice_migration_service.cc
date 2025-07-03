// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/base/signin_switches.h"

DiceMigrationService::DiceMigrationService(Profile* profile)
    : profile_(profile) {
  NOTIMPLEMENTED();
}

DiceMigrationService::~DiceMigrationService() = default;

// static
void DiceMigrationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}
