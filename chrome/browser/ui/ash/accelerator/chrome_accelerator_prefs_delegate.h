// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ACCELERATOR_CHROME_ACCELERATOR_PREFS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_ACCELERATOR_CHROME_ACCELERATOR_PREFS_DELEGATE_H_

#include "ash/accelerators/accelerator_prefs_delegate.h"

// Implementation of ash::AcceleratorPrefsDelegate.
class ChromeAcceleratorPrefsDelegate : public ash::AcceleratorPrefsDelegate {
 public:
  ChromeAcceleratorPrefsDelegate() = default;
  ChromeAcceleratorPrefsDelegate(const ChromeAcceleratorPrefsDelegate&) =
      delete;
  ChromeAcceleratorPrefsDelegate& operator=(
      const ChromeAcceleratorPrefsDelegate&) = delete;
  ~ChromeAcceleratorPrefsDelegate() override = default;

  // ash::AcceleratorPrefsDelegate:
  bool IsUserEnterpriseManaged() const override;
};

#endif  // CHROME_BROWSER_UI_ASH_ACCELERATOR_CHROME_ACCELERATOR_PREFS_DELEGATE_H_
