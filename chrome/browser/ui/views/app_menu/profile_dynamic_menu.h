// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_PROFILE_DYNAMIC_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_PROFILE_DYNAMIC_MENU_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/actions/actions.h"

namespace ui {
class ColorProvider;
}

class Profile;

class ProfileDynamicMenu {
 public:
  explicit ProfileDynamicMenu(BrowserWindowInterface* browser);
  ProfileDynamicMenu(const ProfileDynamicMenu&) = delete;
  ProfileDynamicMenu& operator=(const ProfileDynamicMenu&) = delete;
  ~ProfileDynamicMenu();

  base::WeakPtr<ProfileDynamicMenu> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BuildProfileActions(actions::BaseAction* parent_item);
  void BuildOtherProfiles(actions::BaseAction* parent_item);

 private:
  bool BuildSyncSection(actions::BaseAction* parent_item, Profile* profile);
  void BuildOtherProfilesSection(actions::BaseAction* parent_item,
                                 Profile* profile,
                                 const ui::ColorProvider* color_provider);

  const ui::ColorProvider* GetColorProvider() const;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  base::WeakPtrFactory<ProfileDynamicMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_PROFILE_DYNAMIC_MENU_H_
