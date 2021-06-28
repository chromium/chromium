// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_

#include "components/page_info/page_info_ui.h"
#include "ui/views/view.h"

class ChromePageInfoUiDelegate;
class NonAccessibleImageView;

namespace views {
class Checkbox;
class Label;
class ToggleButton;
}  // namespace views

// The view that is used as a content view of the permissions subpages in page
// info. It contains information about the permission (icon, title, state label)
// and controls to change the permission state (toggle, checkbox and manage
// button).
// *---------------------------------------------------------------*
// | Icon | Title                                         | Toggle |
// |      | State label                                   |        |
// |      |                                               |        |
// |      | "Remember this setting" checkbox              |        |
// |---------------------------------------------------------------|
// | Manage button                                                 |
// *---------------------------------------------------------------*
class PageInfoPermissionContentView : public views::View, public PageInfoUI {
 public:
  PageInfoPermissionContentView(PageInfo* presenter,
                                ChromePageInfoUiDelegate* ui_delegate,
                                ContentSettingsType type);
  ~PageInfoPermissionContentView() override;

  // PageInfoUI implementations.
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;

 private:
  void OnToggleButtonPressed();
  void OnRememberSettingPressed();
  void PermissionChanged();

  PageInfo* presenter_ = nullptr;
  ContentSettingsType type_;
  ChromePageInfoUiDelegate* ui_delegate_ = nullptr;
  PageInfo::PermissionInfo permission_;

  NonAccessibleImageView* icon_ = nullptr;
  views::Label* title_ = nullptr;
  views::Label* state_label_ = nullptr;
  views::ToggleButton* toggle_button_ = nullptr;
  views::Checkbox* remember_setting_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_
