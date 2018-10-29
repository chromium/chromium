// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_SELECTOR_ROW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_SELECTOR_ROW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/browser/ui/page_info/permission_menu_model.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

class Profile;

namespace internal {
class ComboboxModelAdapter;
}  // namespace internal

namespace test {
class PageInfoBubbleViewTestApi;
}

namespace views {
class GridLayout;
class ImageView;
class Label;
class View;
class Combobox;
}  // namespace views

// A |PermissionSelectorRow| is a row in the Page Info bubble that shows a
// permission that a site can have ambient access to, and allows the user to
// control whether that access is granted. A |PermissionSelectorRow| is not
// itself a |View|, but creates some |View|s, which end up owned by the |View|
// hierarchy.
class PermissionSelectorRow {
 public:
  // The |PermissionSelectorRow|'s constituent views are added to |layout|.
  PermissionSelectorRow(Profile* profile,
                        const GURL& url,
                        const PageInfoUI::PermissionInfo& permission,
                        views::GridLayout* layout);
  virtual ~PermissionSelectorRow();

  // Retrieve the minimum height a |PermissionSelectorRow| can be.
  static int MinHeightForPermissionRow();

  void AddObserver(PermissionSelectorRowObserver* observer);

  void PermissionChanged(const PageInfoUI::PermissionInfo& permission);

  // Returns the preferred width for the currently selected combobox option
  // (unchanged by any minimum width set using SetMinComboboxWidth()).
  int GetComboboxWidth() const;

  // Sets the minimum width for |combobox_|.
  void SetMinComboboxWidth(int width);

 private:
  friend class test::PageInfoBubbleViewTestApi;

  void InitializeComboboxView(views::GridLayout* layout,
                              const PageInfoUI::PermissionInfo& permission);

  Profile* profile_;

  // Model for the permission's menu.
  std::unique_ptr<PermissionMenuModel> menu_model_;
  std::unique_ptr<internal::ComboboxModelAdapter> combobox_model_adapter_;

  // These are all owned by the views hierarchy:
  views::ImageView* icon_;
  views::Label* label_;
  views::Combobox* combobox_;

  base::ObserverList<PermissionSelectorRowObserver, false>::Unchecked
      observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PermissionSelectorRow);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_SELECTOR_ROW_H_
