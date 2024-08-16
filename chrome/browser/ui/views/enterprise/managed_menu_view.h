// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Button;
}

class Browser;
class Profile;

// This bubble view is displayed when the user clicks on the management button
// displays the management menu.
class ManagedMenuView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ManagedMenuView, views::BubbleDialogDelegateView)

 public:
  ManagedMenuView(views::Button* anchor_button, Browser* browser);
  ManagedMenuView(const ManagedMenuView&) = delete;
  ManagedMenuView& operator=(const ManagedMenuView&) = delete;

  ~ManagedMenuView() override;

  void RebuildView();
  void BuildView();
  void BuildInfoContainerBackground(const ui::ColorProvider* color_provider);

  // views::BubbleDialogDelegateView:
  void Init() final;
  void OnThemeChanged() override;

  const std::u16string& profile_management_label() const;
  const std::u16string& browser_management_label() const;
  const views::Label* inline_management_title() const;

 private:
  Profile* GetProfile() const;
  void OpenManagementPage();
  void UpdateProfileManagementIcon();
  void UpdateBrowserManagementIcon();
  void SetProfileManagementIcon(const gfx::Image& icon);
  void SetBrowserManagementIcon(const gfx::Image& icon);
  int GetMaxHeight() const;
  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  base::RepeatingCallback<void(const ui::ColorProvider*)>
      info_container_background_callback_ = base::DoNothing();

  raw_ptr<views::View> info_container_ = nullptr;
  raw_ptr<views::Label> inline_title_ = nullptr;
  const raw_ptr<Browser> browser_;
  std::u16string inline_management_title_;
  std::u16string profile_management_label_;
  std::u16string browser_management_label_;
  gfx::Image profile_management_icon_;
  gfx::Image browser_management_icon_;
  PrefChangeRegistrar profile_pref_change_registrar_;
  PrefChangeRegistrar local_state_change_registrar_;

  base::WeakPtrFactory<ManagedMenuView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_VIEW_H_
