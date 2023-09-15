// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_DIALOG_USER_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_DIALOG_USER_OPTIONS_H_

#include "base/functional/callback.h"

namespace web_app {

struct UninstallUserOptions {
  bool user_wants_uninstall = false;
  bool clear_site_data = false;
};

using UninstallDialogCallback =
    base::OnceCallback<void(web_app::UninstallUserOptions)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_DIALOG_USER_OPTIONS_H_
