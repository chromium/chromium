// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"

#include <string>
#include <vector>

#include "apps/saved_files_service.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace {

// Pixel spacing measurements for different parts of the permissions list.
const int kSpacingBetweenBulletAndStartOfText = 5;
const int kSpacingBetweenTextAndRevokeButton = 15;
const int kIndentationBeforeNestedBullet = 13;

// Creates a close button that calls |callback| on click and can be placed to
// the right of a bullet in the permissions list. The alt-text is set to a
// revoke message containing the given |permission_message|.
class RevokeButton : public views::ImageButton {
  METADATA_HEADER(RevokeButton, views::ImageButton)

 public:
  explicit RevokeButton(PressedCallback callback,
                        const std::u16string& permission_message)
      : views::ImageButton(std::move(callback)) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromImage(rb.GetImageNamed(IDR_DISABLE)));
    SetImageModel(views::Button::STATE_HOVERED,
                  ui::ImageModel::FromImage(rb.GetImageNamed(IDR_DISABLE_H)));
    SetImageModel(views::Button::STATE_PRESSED,
                  ui::ImageModel::FromImage(rb.GetImageNamed(IDR_DISABLE_P)));
    SetBorder(std::unique_ptr<views::Border>());
    SetSize(GetPreferredSize());

    // Make the button focusable & give it alt-text so permissions can be
    // revoked using only the keyboard.
    SetRequestFocusOnPress(true);
    SetTooltipText(l10n_util::GetStringFUTF16(
        IDS_APPLICATION_INFO_REVOKE_PERMISSION_ALT_TEXT, permission_message));
  }
  RevokeButton(const RevokeButton&) = delete;
  RevokeButton& operator=(const RevokeButton&) = delete;
  ~RevokeButton() override = default;
};

BEGIN_METADATA(RevokeButton)
END_METADATA

// A bulleted list of permissions.
class BulletedPermissionsList : public views::View {
  METADATA_HEADER(BulletedPermissionsList, views::View)

 public:
  BulletedPermissionsList() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  }
  BulletedPermissionsList(const BulletedPermissionsList&) = delete;
  BulletedPermissionsList& operator=(const BulletedPermissionsList&) = delete;
  ~BulletedPermissionsList() override = default;

  // Given a set of strings for a given permission (|message| for the topmost
  // bullet and a potentially-empty |submessages| for sub-bullets), adds these
  // bullets to the given BulletedPermissionsList. If |revoke_callback| is
  // provided, also adds an X button next to the bullet which calls the callback
  // when clicked.
  void AddPermissionBullets(std::u16string message,
                            const std::vector<std::u16string>& submessages,
                            gfx::ElideBehavior elide_behavior_for_submessages,
                            base::RepeatingClosure revoke_callback) {
    std::unique_ptr<RevokeButton> revoke_button;
    if (!revoke_callback.is_null()) {
      revoke_button = std::make_unique<RevokeButton>(std::move(revoke_callback),
                                                     std::move(message));
    }

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
    auto* row = AddChildView(std::make_unique<views::FlexLayoutView>());
    row->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

    auto* bullet_label =
        row->AddChildView(std::make_unique<views::Label>(u"•"));
    if (is_nested) {
      bullet_label->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, kIndentationBeforeNestedBullet, 0, 0));
    }

    permission_label->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, kSpacingBetweenBulletAndStartOfText, 0, 0));
    permission_label->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded, true));
    row->AddChildView(std::move(permission_label));

    if (revoke_button) {
      revoke_button->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, kSpacingBetweenTextAndRevokeButton, 0, 0));
      row->AddChildView(std::move(revoke_button));
    }
  }
};

BEGIN_METADATA(BulletedPermissionsList)
END_METADATA

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

AppInfoPermissionsPanel::~AppInfoPermissionsPanel() {}

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
    permissions_list->AddPermissionBullets(
        message.message(), message.submessages(), gfx::ELIDE_MIDDLE,
        base::RepeatingClosure());
  }

  // Add USB devices, if the app has any.
  if (GetRetainedDeviceCount() > 0) {
    permissions_list->AddPermissionBullets(
        GetRetainedDeviceHeading(), GetRetainedDevices(), gfx::ELIDE_TAIL,
        base::BindRepeating(&AppInfoPermissionsPanel::RevokeDevicePermissions,
                            base::Unretained(this)));
  }

  // Add retained files, if the app has any.
  if (GetRetainedFileCount() > 0) {
    permissions_list->AddPermissionBullets(
        GetRetainedFileHeading(), GetRetainedFilePaths(), gfx::ELIDE_MIDDLE,
        base::BindRepeating(&AppInfoPermissionsPanel::RevokeFilePermissions,
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
          extensions::mojom::APIPermissionID::kFileSystem)) {
    apps::SavedFilesService* service = apps::SavedFilesService::Get(profile_);
    // The SavedFilesService can be null for incognito profiles. See
    // http://crbug.com/467795.
    if (service) {
      return service->GetAllFileEntries(app_->id()).size();
    }
  }
  return 0;
}

std::u16string AppInfoPermissionsPanel::GetRetainedFileHeading() const {
  return l10n_util::GetPluralStringFUTF16(IDS_APPLICATION_INFO_RETAINED_FILES,
                                          GetRetainedFileCount());
}

std::vector<std::u16string> AppInfoPermissionsPanel::GetRetainedFilePaths()
    const {
  std::vector<std::u16string> retained_file_paths;
  if (app_->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kFileSystem)) {
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
  if (service) {
    service->ClearQueue(app_);
  }
  apps::AppLoadService::Get(profile_)->RestartApplicationIfRunning(app_->id());

  Close();
}

int AppInfoPermissionsPanel::GetRetainedDeviceCount() const {
  return extensions::DevicePermissionsManager::Get(profile_)
      ->GetPermissionMessageStrings(app_->id())
      .size();
}

std::u16string AppInfoPermissionsPanel::GetRetainedDeviceHeading() const {
  return l10n_util::GetPluralStringFUTF16(IDS_APPLICATION_INFO_RETAINED_DEVICES,
                                          GetRetainedDeviceCount());
}

std::vector<std::u16string> AppInfoPermissionsPanel::GetRetainedDevices()
    const {
  return extensions::DevicePermissionsManager::Get(profile_)
      ->GetPermissionMessageStrings(app_->id());
}

void AppInfoPermissionsPanel::RevokeDevicePermissions() {
  extensions::DevicePermissionsManager::Get(profile_)->Clear(app_->id());
  apps::AppLoadService::Get(profile_)->RestartApplicationIfRunning(app_->id());

  Close();
}

BEGIN_METADATA(AppInfoPermissionsPanel)
END_METADATA
