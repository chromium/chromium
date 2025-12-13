// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/model/web_app_comparison.h"

#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os, PendingUpdateComparison value) {
  switch (value) {
    case PendingUpdateComparison::kNotPending:
      return os << "kNotPending";
    case PendingUpdateComparison::kHasPendingAndNotEquals:
      return os << "kHasPendingAndNotEquals";
    case PendingUpdateComparison::kHasPendingAndEquals:
      return os << "kHasPendingAndEquals";
  }
}

WebAppComparison::WebAppComparison() = default;
WebAppComparison::WebAppComparison(const WebAppComparison&) = default;
WebAppComparison& WebAppComparison::operator=(const WebAppComparison&) =
    default;
WebAppComparison::WebAppComparison(WebAppComparison&&) = default;
WebAppComparison& WebAppComparison::operator=(WebAppComparison&&) = default;
WebAppComparison::~WebAppComparison() = default;

bool WebAppComparison::ExistingAppWithoutPendingEqualsNewUpdate() const {
  return name_equality_ && primary_icons_equality_ &&
         shortcut_menu_item_infos_equality_ && other_fields_equality_;
}

bool WebAppComparison::ExistingAppWithPendingEqualsNewUpdate() const {
  bool effective_name_equality;
  if (name_equality_) {
    effective_name_equality =
        pending_name_equality_ == PendingUpdateComparison::kNotPending;
  } else {
    effective_name_equality =
        pending_name_equality_ == PendingUpdateComparison::kHasPendingAndEquals;
  }
  bool effective_primary_icon_equality;
  if (primary_icons_equality_) {
    effective_primary_icon_equality =
        pending_primary_icons_equality_ == PendingUpdateComparison::kNotPending;
  } else {
    effective_primary_icon_equality =
        pending_primary_icons_equality_ ==
        PendingUpdateComparison::kHasPendingAndEquals;
  }
  return effective_name_equality && effective_primary_icon_equality &&
         other_fields_equality_ && shortcut_menu_item_infos_equality_;
}

bool WebAppComparison::IsNameChangeOnly() const {
  return !name_equality_ && primary_icons_equality_ &&
         shortcut_menu_item_infos_equality_ && other_fields_equality_;
}

bool WebAppComparison::IsSecuritySensitiveChangesOnly() const {
  return !name_equality_ && !primary_icons_equality_ &&
         shortcut_menu_item_infos_equality_ && other_fields_equality_;
}

base::DictValue WebAppComparison::ToDict() const {
  return base::DictValue()
      .Set("name_equality", name_equality_)
      .Set("pending_name_equality", base::ToString(pending_name_equality_))
      .Set("primary_icons_equality", primary_icons_equality_)
      .Set("pending_primary_icons_equality",
           base::ToString(pending_primary_icons_equality_))
      .Set("shortcut_menu_item_infos_equality",
           shortcut_menu_item_infos_equality_)
      .Set("other_fields_equality", other_fields_equality_);
}

// static
WebAppComparison WebAppComparison::CompareWebApps(
    const WebApp& existing_web_app,
    const WebAppInstallInfo& new_install_info) {
  CHECK_EQ(existing_web_app.manifest_id(), new_install_info.manifest_id());
  WebAppComparison diff;

  diff.name_equality_ = [&]() {
    std::u16string new_title;
    base::TrimWhitespace(new_install_info.title.value(), base::TRIM_ALL,
                         &new_title);
    return new_title == base::UTF8ToUTF16(existing_web_app.untranslated_name());
  }();
  diff.pending_name_equality_ = [&]() {
    if (!existing_web_app.pending_update_info().has_value() ||
        !existing_web_app.pending_update_info()->has_name()) {
      return PendingUpdateComparison::kNotPending;
    }
    std::u16string new_title;
    base::TrimWhitespace(new_install_info.title.value(), base::TRIM_ALL,
                         &new_title);
    return new_title == base::UTF8ToUTF16(
                            existing_web_app.pending_update_info()->name())
               ? PendingUpdateComparison::kHasPendingAndEquals
               : PendingUpdateComparison::kHasPendingAndNotEquals;
  }();
  diff.primary_icons_equality_ =
      existing_web_app.trusted_icons() == new_install_info.trusted_icons;
  diff.pending_primary_icons_equality_ = [&]() {
    if (!existing_web_app.pending_update_info().has_value() ||
        existing_web_app.pending_update_info()->trusted_icons().empty()) {
      return PendingUpdateComparison::kNotPending;
    }
    std::optional<std::vector<apps::IconInfo>> transformed = ParseAppIconInfos(
        "PendingUpdateInfo",
        existing_web_app.pending_update_info()->trusted_icons());
    if (transformed == new_install_info.trusted_icons) {
      return PendingUpdateComparison::kHasPendingAndEquals;
    }
    return PendingUpdateComparison::kHasPendingAndNotEquals;
  }();
  diff.shortcut_menu_item_infos_equality_ =
      existing_web_app.shortcuts_menu_item_infos() ==
      new_install_info.shortcuts_menu_item_infos;

  diff.other_fields_equality_ = [&]() {
    if (existing_web_app.start_url() != new_install_info.start_url()) {
      return false;
    }
    if (existing_web_app.theme_color() != new_install_info.theme_color) {
      return false;
    }
    if (existing_web_app.scope() != new_install_info.scope) {
      return false;
    }
    if (existing_web_app.display_mode() != new_install_info.display_mode) {
      return false;
    }
    if (existing_web_app.display_mode_override() !=
        new_install_info.display_override) {
      return false;
    }
    if (existing_web_app.share_target() != new_install_info.share_target) {
      return false;
    }
    if (existing_web_app.protocol_handlers() !=
        new_install_info.protocol_handlers) {
      return false;
    }
    if (existing_web_app.note_taking_new_note_url() !=
        new_install_info.note_taking_new_note_url) {
      return false;
    }
    if (existing_web_app.background_color() !=
        new_install_info.background_color) {
      return false;
    }
    if (existing_web_app.dark_mode_theme_color() !=
        new_install_info.dark_mode_theme_color) {
      return false;
    }
    if (existing_web_app.dark_mode_background_color() !=
        new_install_info.dark_mode_background_color) {
      return false;
    }
    if (existing_web_app.launch_handler() != new_install_info.launch_handler) {
      return false;
    }
    if (existing_web_app.permissions_policy() !=
        new_install_info.permissions_policy) {
      return false;
    }
    if (existing_web_app.scope_extensions() !=
        new_install_info.scope_extensions) {
      return false;
    }
    if (existing_web_app.related_applications() !=
        new_install_info.related_applications) {
      return false;
    }
    if (existing_web_app.file_handlers() != new_install_info.file_handlers) {
      return false;
    }
    if (existing_web_app.tab_strip() != new_install_info.tab_strip) {
      return false;
    }
    // Add new manifest properties here to be considered for update.
    return true;
  }();
  return diff;
}

}  // namespace web_app
