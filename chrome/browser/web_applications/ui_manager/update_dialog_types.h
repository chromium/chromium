// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_UI_MANAGER_UPDATE_DIALOG_TYPES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_UI_MANAGER_UPDATE_DIALOG_TYPES_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace web_app {

struct WebAppIdentity {
  WebAppIdentity();
  WebAppIdentity(const std::u16string& title,
                 const gfx::Image& icon,
                 const GURL& start_url);
  ~WebAppIdentity();
  WebAppIdentity(const WebAppIdentity&);
  WebAppIdentity& operator=(const WebAppIdentity&);

  std::u16string title;
  gfx::Image icon;
  GURL start_url;
};

// Represents an update to be presented to the user. Copyable for simplicity.
struct WebAppIdentityUpdate {
  WebAppIdentityUpdate();
  ~WebAppIdentityUpdate();
  WebAppIdentityUpdate(const WebAppIdentityUpdate&);
  WebAppIdentityUpdate& operator=(const WebAppIdentityUpdate&);

  WebAppIdentity MakeOldIdentity() const;
  WebAppIdentity MakeNewIdentity() const;

  std::u16string old_title;
  std::optional<std::u16string> new_title = std::nullopt;
  gfx::Image old_icon;
  std::optional<gfx::Image> new_icon = std::nullopt;
  GURL old_start_url;
  std::optional<GURL> new_start_url = std::nullopt;
};

enum class WebAppIdentityUpdateResult {
  // The user accepted the update.
  kAccept,
  // The user wants to uninstall the app instead of update it.
  kUninstallApp,
  // The user wants to ignore this update.
  kIgnore,
  // The app was uninstalled while the dialog was open, as so it was
  // automatically closed.
  kAppUninstalledDuringDialog,
  // The dialog was closed without user action, likely due to another dialog
  // being present, shutdown, or other factors.
  kUnexpectedError
};

using UpdateReviewDialogCallback =
    base::OnceCallback<void(WebAppIdentityUpdateResult)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_UI_MANAGER_UPDATE_DIALOG_TYPES_H_
