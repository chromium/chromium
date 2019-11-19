// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"

#include <string>
#include <vector>

#include "apps/saved_files_service.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_label.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace {

// IDs for the two bullet column sets.
const int kBulletColumnSetId = 1;
const int kNestedBulletColumnSetId = 2;

// Pixel spacing measurements for different parts of the permissions list.
const int kSpacingBetweenBulletAndStartOfText = 5;
const int kSpacingBetweenTextAndRevokeButton = 15;
const int kIndentationBeforeNestedBullet = 13;

// Creates a close button that calls |callback| on click and can be placed to
// the right of a bullet in the permissions list. The alt-text is set to a
// revoke message containing the given |permission_message|.
class RevokeButton : public views::ImageButton, public views::ButtonListener {
 public:
  explicit RevokeButton(const base::Closure& callback,
                        base::string16 permission_message)
      : views::ImageButton(this), callback_(callback) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    SetImage(views::Button::STATE_NORMAL,
             rb.GetImageNamed(IDR_DISABLE).ToImageSkia());
    SetImage(views::Button::STATE_HOVERED,
             rb.GetImageNamed(IDR_DISABLE_H).ToImageSkia());
    SetImage(views::Button::STATE_PRESSED,
             rb.GetImageNamed(IDR_DISABLE_P).ToImageSkia());
    SetBorder(std::unique_ptr<views::Border>());
    SetSize(GetPreferredSize());

    // Make the button focusable & give it alt-text so permissions can be
    // revoked using only the keyboard.
    SetFocusForPlatform();
    set_request_focus_on_press(true);
    SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_APPLICATION_INFO_REVOKE_PERMISSION_ALT_TEXT, permission_message));
  }
  ~RevokeButton() override {}

 private:
  // Overridden from views::ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(this, sender);
    if (!callback_.is_null())
      callback_.Run();
  }

  const base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(RevokeButton);
};

// A bulleted list of permissions.
// TODO(sashab): Fix BoxLayout to correctly display multi-line strings and then
// remove this class (since the GridLayout will no longer be needed).
class BulletedPermissionsList : public views::View {
 public:
  BulletedPermissionsList() {
    layout_ = SetLayoutManager(std::make_unique<views::GridLayout>());

    // Create 3 columns: the bullet, the bullet text, and the revoke button.
    views::ColumnSet* column_set = layout_->AddColumnSet(kBulletColumnSetId);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 kSpacingBetweenBulletAndStartOfText);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                          1.0 /* stretch to fill space */,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                 kSpacingBetweenTextAndRevokeButton);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                          views::GridLayout::kFixedSize,
                          views::GridLayout::USE_PREF, 0, 0);

    views::ColumnSet* nested_column_set =
        layout_->AddColumnSet(kNestedBulletColumnSetId);
    nested_column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        kIndentationBeforeNestedBullet);
    nested_column_set->AddColumn(
        views::GridLayout::FILL, views::GridLayout::LEADING,
        views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
    nested_column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        kSpacingBetweenBulletAndStartOfText);
    nested_column_set->AddColumn(
        views::GridLayout::FILL, views::GridLayout::LEADING,
        1.0 /* stretch to fill space */, views::GridLayout::USE_PREF, 0, 0);
    nested_column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                        kSpacingBetweenTextAndRevokeButton);
    nested_column_set->AddColumn(
        views::GridLayout::FILL, views::GridLayout::LEADING,
        views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
  }
  ~BulletedPermissionsList() override {}

  // Given a set of strings for a given permission (|message| for the topmost
  // bullet and a potentially-empty |submessages| for sub-bullets), adds these
  // bullets to the given BulletedPermissionsList. If |revoke_callback| is
  // provided, also adds an X button next to the bullet which calls the callback
  // when clicked.
  void AddPermissionBullets(base::string16 message,
                            std::vector<base::string16> submessages,
                            gfx::ElideBehavior elide_behavior_for_submessages,
                            const base::Closure& revoke_callback) {
    std::unique_ptr<RevokeButton> revoke_button;
    if (!revoke_callback.is_null())
      revoke_button = std::make_unique<RevokeButton>(revoke_callback, message);

    auto permission_label = std::make_unique<AppInfoLabel>(message);
    permission_label->SetMultiLine(true);
    AddSinglePermissionBullet(false, std::move(permission_label),
                              std::move(revoke_button));

    for (const auto& submessage : submessages) {
      auto sub_permission_label = std::make_unique<AppInfoLabel>(submessage);
      sub_permission_label->SetElideBehavior(elide_behavior_for_submessages);
      AddSinglePermissionBullet(true, std::move(sub_permission_label), nullptr);
    }
  }

 private:
  void AddSinglePermissionBullet(bool is_nested,
                                 std::unique_ptr<views::Label> permission_label,
                                 std::unique_ptr<RevokeButton> revoke_button) {
    // Add a padding row before every item except the first.
    if (!children().empty()) {
      layout_->AddPaddingRow(views::GridLayout::kFixedSize,
                             ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 views::DISTANCE_RELATED_CONTROL_VERTICAL));
    }

    const base::char16 bullet_point[] = {0x2022, 0};
    auto bullet_label =
        std::make_unique<views::Label>(base::string16(bullet_point));

    layout_->StartRow(
        1.0, is_nested ? kNestedBulletColumnSetId : kBulletColumnSetId);
    layout_->AddView(std::move(bullet_label));
    layout_->AddView(std::move(permission_label));

    if (revoke_button)
      layout_->AddView(std::move(revoke_button));
    else
      layout_->SkipColumns(1);
  }

  views::GridLayout* layout_;

  DISALLOW_COPY_AND_ASSIGN(BulletedPermissionsList);
};

}  // namespace

AppInfoPermissionsPanel::AppInfoPermissionsPanel(
    Profile* profile,
    const extensions::Extension* app)
    : AppInfoPanel(profile, app) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  CreatePermissionsList();
}

AppInfoPermissionsPanel::~AppInfoPermissionsPanel() {
}

void AppInfoPermissionsPanel::CreatePermissionsList() {
  auto permissions_heading = CreateHeading(
      l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_APP_PERMISSIONS_TITLE));
  AddChildView(std::move(permissions_heading));

  if (!HasActivePermissionMessages() && GetRetainedDeviceCount() == 0 &&
      GetRetainedFileCount() == 0) {
    auto no_permissions_text =
        std::make_unique<AppInfoLabel>(l10n_util::GetStringUTF16(
            app_->is_extension()
                ? IDS_APPLICATION_INFO_EXTENSION_NO_PERMISSIONS_TEXT
                : IDS_APPLICATION_INFO_APP_NO_PERMISSIONS_TEXT));
    AddChildView(std::move(no_permissions_text));
    return;
  }

  auto permissions_list = std::make_unique<BulletedPermissionsList>();

  // Add regular and host permission messages.
  for (const auto& message : GetActivePermissionMessages()) {
    permissions_list->AddPermissionBullets(message.message(),
                                           message.submessages(),
                                           gfx::ELIDE_MIDDLE, base::Closure());
  }

  // Add USB devices, if the app has any.
  if (GetRetainedDeviceCount() > 0) {
    permissions_list->AddPermissionBullets(
        GetRetainedDeviceHeading(),
        GetRetainedDevices(),
        gfx::ELIDE_TAIL,
        base::Bind(&AppInfoPermissionsPanel::RevokeDevicePermissions,
                   base::Unretained(this)));
  }

  // Add retained files, if the app has any.
  if (GetRetainedFileCount() > 0) {
    permissions_list->AddPermissionBullets(
        GetRetainedFileHeading(),
        GetRetainedFilePaths(),
        gfx::ELIDE_MIDDLE,
        base::Bind(&AppInfoPermissionsPanel::RevokeFilePermissions,
                   base::Unretained(this)));
  }

  AddChildView(std::move(permissions_list));
}

bool AppInfoPermissionsPanel::HasActivePermissionMessages() const {
  return !GetActivePermissionMessages().empty();
}

extensions::PermissionMessages
AppInfoPermissionsPanel::GetActivePermissionMessages() const {
  return app_->permissions_data()->GetPermissionMessages();
}

int AppInfoPermissionsPanel::GetRetainedFileCount() const {
  if (app_->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kFileSystem)) {
    apps::SavedFilesService* service = apps::SavedFilesService::Get(profile_);
    // The SavedFilesService can be null for incognito profiles. See
    // http://crbug.com/467795.
    if (service)
      return service->GetAllFileEntries(app_->id()).size();
  }
  return 0;
}

base::string16 AppInfoPermissionsPanel::GetRetainedFileHeading() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_APPLICATION_INFO_RETAINED_FILES, GetRetainedFileCount());
}

const std::vector<base::string16>
AppInfoPermissionsPanel::GetRetainedFilePaths() const {
  std::vector<base::string16> retained_file_paths;
  if (app_->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kFileSystem)) {
    apps::SavedFilesService* service = apps::SavedFilesService::Get(profile_);
    // The SavedFilesService can be null for incognito profiles.
    if (service) {
      std::vector<extensions::SavedFileEntry> retained_file_entries =
          service->GetAllFileEntries(app_->id());
      for (std::vector<extensions::SavedFileEntry>::const_iterator it =
               retained_file_entries.begin();
           it != retained_file_entries.end(); ++it) {
        retained_file_paths.push_back(it->path.LossyDisplayName());
      }
    }
  }
  return retained_file_paths;
}

void AppInfoPermissionsPanel::RevokeFilePermissions() {
  apps::SavedFilesService* service = apps::SavedFilesService::Get(profile_);
  // The SavedFilesService can be null for incognito profiles.
  if (service)
    service->ClearQueue(app_);
  apps::AppLoadService::Get(profile_)->RestartApplicationIfRunning(app_->id());

  Close();
}

int AppInfoPermissionsPanel::GetRetainedDeviceCount() const {
  return extensions::DevicePermissionsManager::Get(profile_)
      ->GetPermissionMessageStrings(app_->id())
      .size();
}

base::string16 AppInfoPermissionsPanel::GetRetainedDeviceHeading() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_APPLICATION_INFO_RETAINED_DEVICES, GetRetainedDeviceCount());
}

const std::vector<base::string16> AppInfoPermissionsPanel::GetRetainedDevices()
    const {
  return extensions::DevicePermissionsManager::Get(profile_)
      ->GetPermissionMessageStrings(app_->id());
}

void AppInfoPermissionsPanel::RevokeDevicePermissions() {
  extensions::DevicePermissionsManager::Get(profile_)->Clear(app_->id());
  apps::AppLoadService::Get(profile_)->RestartApplicationIfRunning(app_->id());

  Close();
}
