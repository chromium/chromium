// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_SYSTEM_APP_ACCESSIBLE_NAME_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_SYSTEM_APP_ACCESSIBLE_NAME_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

// An invisible, but accessible label that indicates the system app name. This
// label can only be focused by accessibility features.
class SystemAppAccessibleName : public views::Label {
  METADATA_HEADER(SystemAppAccessibleName, views::Label)

 public:
  explicit SystemAppAccessibleName(const std::u16string& app_name);
  SystemAppAccessibleName(const SystemAppAccessibleName&) = delete;
  SystemAppAccessibleName& operator=(const SystemAppAccessibleName&) = delete;
  ~SystemAppAccessibleName() override;

 private:
  std::u16string app_name_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_SYSTEM_APP_ACCESSIBLE_NAME_H_
