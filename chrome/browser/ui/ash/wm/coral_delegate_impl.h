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

  void OnPostLoginLaunchComplete();

  // ash::CoralDelegate:
  void LaunchPostLoginGroup(coral::mojom::GroupPtr group) override;
  void MoveTabsInGroupToNewDesk(
      const std::vector<coral::mojom::Tab>& tabs) override;
  // TODO(sammiequon): Saved desk operations may be able to be fully done in
  // ash.
  void CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) override;

 private:
  // Handles launching apps and creating browsers for post login groups.
  std::unique_ptr<DesksTemplatesAppLaunchHandler> app_launch_handler_;

  base::WeakPtrFactory<CoralDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_WM_CORAL_DELEGATE_IMPL_H_
