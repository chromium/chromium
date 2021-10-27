// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_H_

#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "components/page_info/page_info_ui.h"
#include "ui/views/view.h"

class ChromePageInfoUiDelegate;
class PageInfoRowView;
class PageInfoNavigationHandler;

namespace views {
class Label;
class ToggleButton;
}  // namespace views

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

// A view that shows a permission that a site is able to access, and
// allows the user to control via toggle whether that access is granted. Has a
// button that opens a subpage with more controls.
class PermissionToggleRowView : public views::View {
 public:
  PermissionToggleRowView(ChromePageInfoUiDelegate* delegate,
                          PageInfoNavigationHandler* navigation_handler,
                          const PageInfo::PermissionInfo& permission,
                          bool should_show_spacer_view);
  PermissionToggleRowView(const PermissionToggleRowView&) = delete;
  PermissionToggleRowView& operator=(const PermissionToggleRowView&) = delete;

  ~PermissionToggleRowView() override;

  void AddObserver(PermissionSelectorRowObserver* observer);
  void PermissionChanged();
  void ResetPermission();

 private:
  friend class test::PageInfoBubbleViewTestApi;

  void OnToggleButtonPressed();
  void InitForUserSource(bool should_show_spacer_view);
  void InitForManagedSource(ChromePageInfoUiDelegate* delegate);
  void UpdateUiOnPermissionChanged();

  PageInfo::PermissionInfo permission_;

  PageInfoRowView* row_view_ = nullptr;
  views::Label* state_label_ = nullptr;
  views::ToggleButton* toggle_button_ = nullptr;
  views::View* spacer_view_ = nullptr;

  ChromePageInfoUiDelegate* delegate_ = nullptr;
  PageInfoNavigationHandler* navigation_handler_ = nullptr;

  base::ObserverList<PermissionSelectorRowObserver, false>::Unchecked
      observer_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_H_
