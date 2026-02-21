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
  static constexpr int kNameChange = 0b001;
  static constexpr int kIconChange = 0b010;
  static constexpr int kUrlChange = 0b100;

  WebAppIdentityUpdate();
  ~WebAppIdentityUpdate();
  WebAppIdentityUpdate(const WebAppIdentityUpdate&);
  WebAppIdentityUpdate& operator=(const WebAppIdentityUpdate&);

  WebAppIdentity MakeOldIdentity() const;
  WebAppIdentity MakeNewIdentity() const;

  // Returns an unique combination represented by binary integers that determine
  // exactly what changed in `WebAppIdentityUpdate` using the following
  // guidelines:
  // 1. Title changes only are denoted by 0b001.
  // 2. Icon changes only are denoted by 0b010.
  // 3. Url changes only are denoted by 0b100.
  // 4. A combination of changes are denoted by setting each flag in their
  // correct position accordingly.
  int GetCombinationChangeIndex() const;

  // Returns true if the identity update has a title change.
  bool HasTitleChange() const;

  // If the `new_*` fields are std::nullopt, then they are considered to be the
  // same as the `old_*` fields.
  std::u16string old_title;
  std::optional<std::u16string> new_title = std::nullopt;
  gfx::Image old_icon;
  std::optional<gfx::Image> new_icon = std::nullopt;
  GURL old_start_url;
  std::optional<GURL> new_start_url = std::nullopt;

  // To be used for forced app migrations to ensure that the user cannot ignore
  // this update. If this is true, `new_start_url` NEEDS to be set and be
  // different from `old_start_url`.
  bool is_forced_migration = false;
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
  // The dialog was closed by direct user action (e.g., the escape key), and
  // because the dialog requires immediate action, close the web app entirely.
  kCloseApp = 5,
  kMaxValue = kCloseApp
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppIdentityUpdateResult)

using UpdateReviewDialogCallback =
    base::OnceCallback<void(WebAppIdentityUpdateResult)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_UI_MANAGER_UPDATE_DIALOG_TYPES_H_
