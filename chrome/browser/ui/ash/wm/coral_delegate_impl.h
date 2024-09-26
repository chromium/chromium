// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_

#include "ash/public/cpp/coral_delegate.h"

class DesksTemplatesAppLaunchHandler;

class CoralDelegateImpl : public ash::CoralDelegate {
 public:
  CoralDelegateImpl();
  CoralDelegateImpl(const CoralDelegateImpl&) = delete;
  CoralDelegateImpl& operator=(const CoralDelegateImpl&) = delete;
  ~CoralDelegateImpl() override;

  // ash::CoralDelegateImpl:
  void LaunchPostLoginGroup(coral::mojom::GroupPtr group) override;
  void MoveTabsInGroupToNewDesk(coral::mojom::GroupPtr group) override;
  void CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) override;

 private:
  std::unique_ptr<DesksTemplatesAppLaunchHandler> app_launch_handler_;
};

#endif  // CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_
