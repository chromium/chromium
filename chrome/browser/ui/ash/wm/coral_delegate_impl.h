// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_

#include "ash/public/cpp/coral_delegate.h"

class CoralDelegateImpl : public ash::CoralDelegate {
 public:
  CoralDelegateImpl();
  CoralDelegateImpl(const CoralDelegateImpl&) = delete;
  CoralDelegateImpl& operator=(const CoralDelegateImpl&) = delete;
  ~CoralDelegateImpl() override;

  // ash::CoralDelegateImpl:
  void LaunchPostLoginCluster(
      const ash::coral_util::CoralCluster& cluster) override;
  void OpenNewDeskWithCluster(
      const ash::coral_util::CoralCluster& cluster) override;
  void CreateSavedDeskFromCluster(
      const ash::coral_util::CoralCluster& cluster) override;
};

#endif  // CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_
