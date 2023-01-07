// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CALLBACK_APP_IDENTITY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CALLBACK_APP_IDENTITY_H_

#include "base/functional/callback.h"

namespace web_app {

// As part of evaluating a manifest update, the flow needs to take into
// consideration whether the app identity (name and icon) is changing and
// whether to allow that.
enum class AppIdentityUpdate {
  kAllowed = 0,
  kUninstall,
  kSkipped,
};

using AppIdentityDialogCallback = base::OnceCallback<void(AppIdentityUpdate)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_CALLBACK_APP_IDENTITY_H_
