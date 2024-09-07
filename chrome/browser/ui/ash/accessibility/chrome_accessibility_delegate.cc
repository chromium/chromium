// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/chrome_accessibility_delegate.h"

#include <limits>

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"

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
  if (MagnificationManager::Get())
    MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
}

double ChromeAccessibilityDelegate::GetSavedScreenMagnifierScale() {
  if (MagnificationManager::Get())
    return MagnificationManager::Get()->GetSavedScreenMagnifierScale();

  return std::numeric_limits<double>::min();
}
