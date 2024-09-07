// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view_observer.h"
#include "components/page_info/page_info_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

class ChromePageInfoUiDelegate;
class PageInfoNavigationHandler;

namespace views {
class ToggleButton;
class StyledLabel;
}  // namespace views

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

// A view that shows a permission that a site is able to access, and
// allows the user to control via toggle whether that access is granted. Has a
// button that opens a subpage with more controls.
class PermissionToggleRowView : public views::View {
  METADATA_HEADER(PermissionToggleRowView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRowSubTitleCameraElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRowSubTitleMicrophoneElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kPermissionDisabledAtSystemLevelElementId);
  PermissionToggleRowView(ChromePageInfoUiDelegate* delegate,
                          PageInfoNavigationHandler* navigation_handler,
                          const PageInfo::PermissionInfo& permission,
                          bool should_show_spacer_view);
  PermissionToggleRowView(const PermissionToggleRowView&) = delete;
  PermissionToggleRowView& operator=(const PermissionToggleRowView&) = delete;

  ~PermissionToggleRowView() override;

  void AddObserver(PermissionToggleRowViewObserver* observer);
  void PermissionChanged();
  void UpdatePermission(const PageInfo::PermissionInfo& permission);
  void ResetPermission();

  const std::u16string& GetRowTitleForTesting() const {
    return row_view_->GetTitleForTesting();
  }

  std::u16string GetRowSubTitleForTesting() const {
    return state_label_ != nullptr ? state_label_->GetText() : u"";
  }

  views::ToggleButton* toggle_button_for_testing() { return toggle_button_; }

  bool GetToggleButtonStateForTesting() const;

 private:
  friend class test::PageInfoBubbleViewTestApi;

  void OnToggleButtonPressed();
  void InitForUserSource(bool should_show_spacer_view,
                         const std::u16string& toggle_accessible_name);
  void InitForManagedSource(ChromePageInfoUiDelegate* delegate);
  void UpdateUiOnPermissionChanged();

  PageInfo::PermissionInfo permission_;
  bool permission_blocked_on_system_level_ = false;

  raw_ptr<RichControlsContainerView, DanglingUntriaged> row_view_ = nullptr;
  raw_ptr<views::Label, DanglingUntriaged> state_label_ = nullptr;
  raw_ptr<views::StyledLabel, DanglingUntriaged>
      blocked_on_system_level_label_ = nullptr;
  raw_ptr<views::ToggleButton, DanglingUntriaged> toggle_button_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> spacer_view_ = nullptr;

  raw_ptr<ChromePageInfoUiDelegate, DanglingUntriaged> delegate_ = nullptr;
  raw_ptr<PageInfoNavigationHandler, DanglingUntriaged> navigation_handler_ =
      nullptr;

  base::ObserverList<PermissionToggleRowViewObserver, false>::Unchecked
      observer_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_H_
