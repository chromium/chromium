// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_PERMISSION_MENU_MODEL_H_
#define CHROME_BROWSER_UI_PAGE_INFO_PERMISSION_MENU_MODEL_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/page_info_ui.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class PermissionMenuModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  typedef base::RepeatingCallback<void(const PageInfo::PermissionInfo&)>
      ChangeCallback;

  // Create a new menu model for permission settings.
  PermissionMenuModel(PageInfoUiDelegate* delegate,
                      const PageInfo::PermissionInfo& info,
                      ChangeCallback callback);
  ~PermissionMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int encoded_command_id) const override;
  bool IsCommandIdEnabled(int encoded_command_id) const override;
  void ExecuteCommand(int encoded_command_id, int event_flags) override;

 private:
  // The permission info represented by the menu model.
  PageInfo::PermissionInfo permission_;

  // Callback to be called when the permission's setting is changed.
  ChangeCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMenuModel);
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_PERMISSION_MENU_MODEL_H_
