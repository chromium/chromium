// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/chrome_accessibility_delegate.h"

#include <limits>

#include "ash/accessibility/accessibility_prefs_custom_associator.h"
#include "base/check.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_context.h"

using ash::AccessibilityManager;
using ash::MagnificationManager;

ChromeAccessibilityDelegate::ChromeAccessibilityDelegate() = default;

ChromeAccessibilityDelegate::~ChromeAccessibilityDelegate() = default;

void ChromeAccessibilityDelegate::SetMagnifierEnabled(bool enabled) {
  CHECK(MagnificationManager::Get(), base::NotFatalUntil::M162);
  return MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

bool ChromeAccessibilityDelegate::IsMagnifierEnabled() const {
  CHECK(MagnificationManager::Get(), base::NotFatalUntil::M162);
  return MagnificationManager::Get()->IsMagnifierEnabled();
}

bool ChromeAccessibilityDelegate::ShouldShowAccessibilityMenu() const {
  CHECK(AccessibilityManager::Get(), base::NotFatalUntil::M162);
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
  // TODO(crbug.com/278643115): Take `account_id` from the callers.
  const session_manager::Session* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  CHECK(active_session);
  content::BrowserContext* browser_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          active_session->account_id());
  CHECK(browser_context);
  sync_preferences::PrefServiceSyncable* pref_service_syncable =
      PrefServiceSyncableFromProfile(
          Profile::FromBrowserContext(browser_context));
  if (pref_service_syncable == pref_service) {
    return std::make_unique<ash::AccessibilityPrefsCustomAssociator>(
        pref_service_syncable);
  }
  return nullptr;
}
