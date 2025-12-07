// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_WEB_APP_COMPARISON_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_WEB_APP_COMPARISON_H_

#include <iosfwd>

namespace base {
class DictValue;
}

namespace web_app {

class WebApp;
struct WebAppInstallInfo;

enum class PendingUpdateComparison {
  // There is no pending update info on the existing app.
  kNotPending,
  // There is pending update info on the existing app, but it does not equal the
  // new info.
  kHasPendingAndNotEquals,
  // There is pending update info on the existing app, and it equals the new
  // info.
  kHasPendingAndEquals,
};

std::ostream& operator<<(std::ostream& os, PendingUpdateComparison value);

// This class is a result of comparing a `WebApp` on disk with new
// `WebAppInstallInfo` from a web page. It contains information about what is
// different between the two.
//
// The information is pretty specific to web app update operations, which need
// to reason about security vs non-security fields, pending update info, and
// icon information matching.
class WebAppComparison {
 public:
  WebAppComparison();
  WebAppComparison(const WebAppComparison&);
  WebAppComparison& operator=(const WebAppComparison&);
  WebAppComparison(WebAppComparison&&);
  WebAppComparison& operator=(WebAppComparison&&);
  ~WebAppComparison();

  static WebAppComparison CompareWebApps(
      const WebApp& existing_web_app,
      const WebAppInstallInfo& new_install_info);

  // Returns if the `existing_web_app` name equals the `new_install_info` name.
  bool name_equality() const { return name_equality_; }

  // The same comparison as `name_equality()` but with the
  // `existing_web_app` pending update info.
  PendingUpdateComparison pending_name_equality() const {
    return pending_name_equality_;
  }

  // Returns if the `existing_web_app` primary icon entries equal the
  // `new_install_info` primary icon entries. This *does not* include
  // downloading and looking at the bitmap data, just the entries with the urls.
  bool primary_icons_equality() const { return primary_icons_equality_; }

  // The same comparison as `primary_icons_equality()` but with the
  // `existing_web_app` pending update info.
  PendingUpdateComparison pending_primary_icons_equality() const {
    return pending_primary_icons_equality_;
  }

  // Returns if the `existing_web_app` shortcut menu item infos equal the
  // `new_install_info` shortcut menu item infos. This *does not* include
  // downloading and looking at the bitmap data, just the entries with the urls.
  bool shortcut_menu_item_infos_equality() const {
    return shortcut_menu_item_infos_equality_;
  }

  // Returns if all other fields besides name, icons, and shortcut menu items
  // are equal.
  bool other_fields_equality() const { return other_fields_equality_; }

  // Returns if the existing app configuration (not considering any pending
  // update info) matches the `new_install_info`.
  bool ExistingAppWithoutPendingEqualsNewUpdate() const;

  // Return if the existing app configuration, with any pending update info
  // applied, matches the `new_install_info`.
  bool ExistingAppWithPendingEqualsNewUpdate() const;

  // Returns true if the only thing that has changed is the name of the app.
  // This does not consider any pending update info.
  bool IsNameChangeOnly() const;

  // Returns true if the only things that have changed are security sensitive
  // fields (name and icons).
  // This does not consider any pending update info.
  bool IsSecuritySensitiveChangesOnly() const;

  // Returns a `base::Value::Dict` representation of this object, useful for
  // debugging.
  base::DictValue ToDict() const;

 private:
  bool name_equality_ = false;
  PendingUpdateComparison pending_name_equality_ =
      PendingUpdateComparison::kNotPending;

  bool primary_icons_equality_ = false;
  PendingUpdateComparison pending_primary_icons_equality_ =
      PendingUpdateComparison::kNotPending;

  bool shortcut_menu_item_infos_equality_ = false;
  bool other_fields_equality_ = false;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_WEB_APP_COMPARISON_H_
