// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "ipc/ipc_message.h"

using extensions::api::manifest_types::ChromeUIOverrides;

namespace extensions {

// The manifest permission implementation supports a permission for overriding
// the bookmark UI.
class UIOverridesHandler::ManifestPermissionImpl : public ManifestPermission {
 public:
  explicit ManifestPermissionImpl(bool override_bookmarks_ui_permission)
      : override_bookmarks_ui_permission_(override_bookmarks_ui_permission) {}

  // extensions::ManifestPermission overrides.
  std::string name() const override { return manifest_keys::kUIOverride; }

  std::string id() const override { return name(); }

  PermissionIDSet GetPermissions() const override {
    PermissionIDSet permissions;
    if (override_bookmarks_ui_permission_) {
      // TODO(sashab): Add rule to ChromePermissionMessageProvider:
      // kOverrideBookmarksUI ->
      // IDS_EXTENSION_PROMPT_WARNING_OVERRIDE_BOOKMARKS_UI
      permissions.insert(APIPermission::kOverrideBookmarksUI);
    }
    return permissions;
  }

  bool FromValue(const base::Value* value) override {
    return value && value->GetAsBoolean(&override_bookmarks_ui_permission_);
  }

  std::unique_ptr<base::Value> ToValue() const override {
    return std::unique_ptr<base::Value>(
        new base::Value(override_bookmarks_ui_permission_));
  }

  std::unique_ptr<ManifestPermission> Diff(
      const ManifestPermission* rhs) const override {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return std::make_unique<ManifestPermissionImpl>(
        override_bookmarks_ui_permission_ &&
        !other->override_bookmarks_ui_permission_);
  }

  std::unique_ptr<ManifestPermission> Union(
      const ManifestPermission* rhs) const override {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return std::make_unique<ManifestPermissionImpl>(
        override_bookmarks_ui_permission_ ||
        other->override_bookmarks_ui_permission_);
  }

  std::unique_ptr<ManifestPermission> Intersect(
      const ManifestPermission* rhs) const override {
    const ManifestPermissionImpl* other =
        static_cast<const ManifestPermissionImpl*>(rhs);

    return std::make_unique<ManifestPermissionImpl>(
        override_bookmarks_ui_permission_ &&
        other->override_bookmarks_ui_permission_);
  }

 private:
  bool override_bookmarks_ui_permission_;
};

UIOverrides::UIOverrides() {}

UIOverrides::~UIOverrides() {}

const UIOverrides* UIOverrides::Get(const Extension* extension) {
  return static_cast<UIOverrides*>(
      extension->GetManifestData(manifest_keys::kUIOverride));
}

bool UIOverrides::RemovesBookmarkButton(const Extension* extension) {
  const UIOverrides* ui_overrides = Get(extension);
  return ui_overrides && ui_overrides->bookmarks_ui &&
      ui_overrides->bookmarks_ui->remove_button &&
      *ui_overrides->bookmarks_ui->remove_button;
}

bool UIOverrides::RemovesBookmarkShortcut(const Extension* extension) {
  const UIOverrides* ui_overrides = Get(extension);
  return ui_overrides && ui_overrides->bookmarks_ui &&
      ui_overrides->bookmarks_ui->remove_bookmark_shortcut &&
      *ui_overrides->bookmarks_ui->remove_bookmark_shortcut;
}

bool UIOverrides::RemovesBookmarkAllTabsShortcut(const Extension* extension) {
  const UIOverrides* ui_overrides = Get(extension);
  return ui_overrides && ui_overrides->bookmarks_ui &&
      ui_overrides->bookmarks_ui->remove_bookmark_open_pages_shortcut &&
      *ui_overrides->bookmarks_ui->remove_bookmark_open_pages_shortcut;
}

UIOverridesHandler::UIOverridesHandler() {}

UIOverridesHandler::~UIOverridesHandler() {}

bool UIOverridesHandler::Parse(Extension* extension, base::string16* error) {
  const base::Value* dict = NULL;
  CHECK(extension->manifest()->Get(manifest_keys::kUIOverride, &dict));
  std::unique_ptr<ChromeUIOverrides> overrides(
      ChromeUIOverrides::FromValue(*dict, error));
  if (!overrides)
    return false;

  std::unique_ptr<UIOverrides> info(new UIOverrides);
  info->bookmarks_ui.swap(overrides->bookmarks_ui);
  if (!info->bookmarks_ui) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidEmptyDictionary,
        manifest_keys::kUIOverride);
    return false;
  }
  info->manifest_permission.reset(new ManifestPermissionImpl(
      info->bookmarks_ui.get() != NULL));
  extension->SetManifestData(manifest_keys::kUIOverride, std::move(info));
  return true;
}

bool UIOverridesHandler::Validate(const Extension* extension,
                                  std::string* error,
                                  std::vector<InstallWarning>* warnings) const {
  const UIOverrides* ui_overrides = UIOverrides::Get(extension);

  if (ui_overrides && ui_overrides->bookmarks_ui) {
    if (!FeatureSwitch::enable_override_bookmarks_ui()->IsEnabled()) {
      warnings->push_back(InstallWarning(
          ErrorUtils::FormatErrorMessage(
              manifest_errors::kUnrecognizedManifestProperty,
              manifest_keys::kBookmarkUI,
              manifest_keys::kUIOverride)));
    }
  }

  return true;
}

ManifestPermission* UIOverridesHandler::CreatePermission() {
  return new ManifestPermissionImpl(false);
}

ManifestPermission* UIOverridesHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  const UIOverrides* data = UIOverrides::Get(extension);
  if (data)
    return data->manifest_permission->Clone().release();
  return NULL;
}
base::span<const char* const> UIOverridesHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kUIOverride};
  return kKeys;
}

}  // namespace extensions
