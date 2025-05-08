// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_

#include "components/password_manager/core/common/password_manager_ui.h"

// An interface for updating the passwords icon in the location bar.
class ManagePasswordsIconView {
 public:
  ManagePasswordsIconView() = default;

  ManagePasswordsIconView(const ManagePasswordsIconView&) = delete;
  ManagePasswordsIconView& operator=(const ManagePasswordsIconView&) = delete;

  virtual void SetState(password_manager::ui::State state,
                        bool is_blocklisted) = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_VIEW_H_
