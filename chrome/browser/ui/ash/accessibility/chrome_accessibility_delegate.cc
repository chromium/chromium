// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/chrome_accessibility_delegate.h"

#include <limits>

#include "ash/accessibility/accessibility_prefs_custom_associator.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"

using ash::AccessibilityManager;
using ash::MagnificationManager;

ChromeAccessibilityDelegate::ChromeAccessibilityDelegate() = default;

ChromeAccessibilityDelegate::~ChromeAccessibilityDelegate() = default;

void ChromeAccessibilityDelegate::SetMagnifierEnabled(bool enabled) {
  DCHECK(MagnificationManager::Get());
  return MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

bool ChromeAccessibilityDelegate::IsMagnifierEnabled() const {
  DCHECK(MagnificationManager::Get());
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

bool ChromeAccessibilityDelegate::ShouldShowAccessibilityMenu() const {
  DCHECK(AccessibilityManager::Get());
  return AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
}

void ChromeAccessibilityDelegate::SaveScreenMagnifierScale(double scale) {
  if (MagnificationManager::Get()) {
    MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
  }
}

double ChromeAccessibilityDelegate::GetSavedScreenMagnifierScale() {
  if (MagnificationManager::Get()) {
    return MagnificationManager::Get()->GetSavedScreenMagnifierScale();
  }

  return std::numeric_limits<double>::min();
}

std::unique_ptr<ash::AccessibilityPrefsCustomAssociator>
ChromeAccessibilityDelegate::CreatePrefsCustomAssociator(
    PrefService* pref_service) {
  sync_preferences::PrefServiceSyncable* pref_service_syncable =
      PrefServiceSyncableFromProfile(ProfileManager::GetActiveUserProfile());
  if (pref_service_syncable == pref_service) {
    return std::make_unique<ash::AccessibilityPrefsCustomAssociator>(
        pref_service_syncable);
  }
  return nullptr;
}
