// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "components/page_info/page_info_ui.h"
#include "ui/views/view.h"

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
#include "chrome/browser/ui/views/media_preview/media_coordinator.h"
#endif

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
  // views::View overrides
  void ChildPreferredSizeChanged(views::View* child) override;

  void OnToggleButtonPressed();
  void OnRememberSettingPressed();
  void PermissionChanged();

  // Adds Media (Camera or Mic) live preview feeds.
  void MaybeAddMediaPreview();

  raw_ptr<PageInfo> presenter_ = nullptr;
  ContentSettingsType type_;
  raw_ptr<ChromePageInfoUiDelegate> ui_delegate_ = nullptr;
  PageInfo::PermissionInfo permission_;

  raw_ptr<NonAccessibleImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> state_label_ = nullptr;
  raw_ptr<views::ToggleButton> toggle_button_ = nullptr;
  raw_ptr<views::Checkbox> remember_setting_ = nullptr;

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
  std::optional<MediaCoordinator> media_preview_coordinator_;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_
