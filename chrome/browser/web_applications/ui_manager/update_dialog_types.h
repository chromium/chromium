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

inline constexpr int kIconSizeForUpdateDialog = 96;

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

// The result of the predictable app updating dialog closing, either from an
// explicit user action or a system behavior.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WebAppIdentityUpdateResult)
enum class WebAppIdentityUpdateResult {
  // The user accepted the update.
  kAccept = 0,
  // The user wants to uninstall the app instead of update it.
  kUninstallApp = 1,
  // The user wants to ignore this update.
  kIgnore = 2,
  // The app was uninstalled while the dialog was open, as so it was
  // automatically closed.
  kAppUninstalledDuringDialog = 3,
  // The dialog was closed without user action, likely due to another dialog
  // being present, shutdown, or other factors.
  kUnexpectedError = 4,
  kMaxValue = kUnexpectedError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppIdentityUpdateResult)

using UpdateReviewDialogCallback =
    base::OnceCallback<void(WebAppIdentityUpdateResult)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_UI_MANAGER_UPDATE_DIALOG_TYPES_H_
