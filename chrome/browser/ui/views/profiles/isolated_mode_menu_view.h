// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_ISOLATED_MODE_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_ISOLATED_MODE_MENU_VIEW_H_

#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "ui/views/bubble/bubble_anchor.h"

class BrowserWindowInterface;

// This bubble view is displayed when the user clicks on the avatar button in
// Enterprise Isolated Mode.
class IsolatedModeMenuView : public ProfileMenuViewBase {
 public:
  // `browser` must not be nullptr.
  IsolatedModeMenuView(views::BubbleAnchor anchor_element,
                       BrowserWindowInterface* browser);

  IsolatedModeMenuView(const IsolatedModeMenuView&) = delete;
  IsolatedModeMenuView& operator=(const IsolatedModeMenuView&) = delete;

  ~IsolatedModeMenuView() override;

  // ProfileMenuViewBase:
  void BuildMenu() override;

 private:
  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  // Button actions.
  void OnProfileManagementButtonClicked();
  void OnExitButtonClicked();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_ISOLATED_MODE_MENU_VIEW_H_
