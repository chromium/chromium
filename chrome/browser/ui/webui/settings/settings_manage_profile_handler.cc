// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace settings {

namespace {

const char kProfileShortcutSettingHidden[] = "profileShortcutSettingHidden";
const char kProfileShortcutFound[] = "profileShortcutFound";
const char kProfileShortcutNotFound[] = "profileShortcutNotFound";

}  // namespace

ManageProfileHandler::ManageProfileHandler(Profile* profile)
    : profile_(profile) {}

ManageProfileHandler::~ManageProfileHandler() {}

void ManageProfileHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAvailableIcons",
      base::BindRepeating(&ManageProfileHandler::HandleGetAvailableIcons,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setProfileIconToGaiaAvatar",
      base::BindRepeating(
          &ManageProfileHandler::HandleSetProfileIconToGaiaAvatar,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setProfileIconToDefaultAvatar",
      base::BindRepeating(
          &ManageProfileHandler::HandleSetProfileIconToDefaultAvatar,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setProfileName",
      base::BindRepeating(&ManageProfileHandler::HandleSetProfileName,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestProfileShortcutStatus",
      base::BindRepeating(
          &ManageProfileHandler::HandleRequestProfileShortcutStatus,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addProfileShortcut",
      base::BindRepeating(&ManageProfileHandler::HandleAddProfileShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeProfileShortcut",
      base::BindRepeating(&ManageProfileHandler::HandleRemoveProfileShortcut,
                          base::Unretained(this)));
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
}

void ManageProfileHandler::OnJavascriptAllowed() {
  observation_.Observe(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());
}

void ManageProfileHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void ManageProfileHandler::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  if (profile_path != profile_->GetPath())
    return;

  // GAIA image is loaded asynchronously.
  FireWebUIListener("available-icons-changed", *GetAvailableIcons());
}

void ManageProfileHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  if (profile_path != profile_->GetPath())
    return;

  // This is necessary to send the potentially updated GAIA photo.
  FireWebUIListener("available-icons-changed", *GetAvailableIcons());
}

void ManageProfileHandler::OnProfileThemeColorsChanged(
    const base::FilePath& profile_path) {
  // This is necessary to send the potentially updated Generic colored avatar.
  OnProfileAvatarChanged(profile_path);
}

void ManageProfileHandler::HandleGetAvailableIcons(
    const base::ListValue* args) {
  AllowJavascript();

  profiles::UpdateGaiaProfileInfoIfNeeded(profile_);

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(*callback_id, *GetAvailableIcons());
}

std::unique_ptr<base::ListValue> ManageProfileHandler::GetAvailableIcons() {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  // TODO(msalama): Convert to a DCHECK.
  if (!entry) {
    LOG(ERROR) << "No profile attributes entry found for profile with path: "
               << profile_->GetPath();
    return std::make_unique<base::ListValue>();
  }

  bool using_gaia = entry->IsUsingGAIAPicture();
  size_t selected_avatar_idx =
      using_gaia ? SIZE_MAX : entry->GetAvatarIconIndex();

  // Obtain a list of the modern avatar icons.
  std::unique_ptr<base::ListValue> avatars(
      profiles::GetCustomProfileAvatarIconsAndLabels(selected_avatar_idx));

  if (entry->GetSigninState() == SigninState::kNotSignedIn) {
    ProfileThemeColors colors = entry->GetProfileThemeColors();
    auto generic_avatar_info = profiles::GetDefaultProfileAvatarIconAndLabel(
        colors.default_avatar_fill_color, colors.default_avatar_stroke_color,
        selected_avatar_idx == profiles::GetPlaceholderAvatarIndex());
    avatars->Insert(0, std::move(generic_avatar_info));
    return avatars;
  }

  // Add the GAIA picture to the beginning of the list if it is available.
  const gfx::Image* icon = entry->GetGAIAPicture();
  if (icon) {
    gfx::Image avatar_icon = profiles::GetAvatarIconForWebUI(*icon, true);
    auto gaia_picture_info = profiles::GetAvatarIconAndLabelDict(
        /*url=*/webui::GetBitmapDataUrl(avatar_icon.AsBitmap()),
        /*label=*/
        l10n_util::GetStringUTF16(IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO),
        /*index=*/0, using_gaia, /*is_gaia_avatar=*/true);
    avatars->Insert(0, std::move(gaia_picture_info));
  }

  return avatars;
}

void ManageProfileHandler::HandleSetProfileIconToGaiaAvatar(
    const base::ListValue* /* args */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefService* pref_service = profile_->GetPrefs();
  bool previously_using_gaia_icon =
      pref_service->GetBoolean(prefs::kProfileUsingGAIAAvatar);

  pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar, false);
  pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, true);
  if (!previously_using_gaia_icon) {
    // Only log if they changed to the GAIA photo.
    // Selection of GAIA photo as avatar is logged as part of the function
    // below.
    ProfileMetrics::LogProfileSwitchGaia(ProfileMetrics::GAIA_OPT_IN);
  }

  ProfileMetrics::LogProfileUpdate(profile_->GetPath());
}

void ManageProfileHandler::HandleSetProfileIconToDefaultAvatar(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(args);
  CHECK_EQ(1u, args->GetSize());
  CHECK(args->GetList()[0].is_int());

  size_t new_icon_index = args->GetList()[0].GetInt();
  CHECK(profiles::IsDefaultAvatarIconIndex(new_icon_index));

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInteger(prefs::kProfileAvatarIndex, new_icon_index);
  pref_service->SetBoolean(
      prefs::kProfileUsingDefaultAvatar,
      new_icon_index == profiles::GetPlaceholderAvatarIndex());
  pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, false);

  ProfileMetrics::LogProfileAvatarSelection(new_icon_index);
  ProfileMetrics::LogProfileUpdate(profile_->GetPath());
}

void ManageProfileHandler::HandleSetProfileName(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(args);
  CHECK_EQ(1u, args->GetSize());

  std::u16string new_profile_name;
  CHECK(args->GetString(0, &new_profile_name));

  base::TrimWhitespace(new_profile_name, base::TRIM_ALL, &new_profile_name);
  CHECK(!new_profile_name.empty());
  profiles::UpdateProfileName(profile_, new_profile_name);

  ProfileMetrics::LogProfileUpdate(profile_->GetPath());
}

void ManageProfileHandler::HandleRequestProfileShortcutStatus(
    const base::ListValue* args) {
  AllowJavascript();
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());

  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  // Don't show the add/remove desktop shortcut button in the single user case.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  if (storage.GetNumberOfProfiles() <= 1u) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(kProfileShortcutSettingHidden));
    return;
  }

  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  DCHECK(shortcut_manager);
  shortcut_manager->HasProfileShortcuts(
      profile_->GetPath(),
      base::BindOnce(&ManageProfileHandler::OnHasProfileShortcuts,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void ManageProfileHandler::OnHasProfileShortcuts(
    const std::string& callback_id, bool has_shortcuts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(has_shortcuts ? kProfileShortcutFound
                                : kProfileShortcutNotFound));
}

void ManageProfileHandler::HandleAddProfileShortcut(
    const base::ListValue* args) {
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());
  ProfileShortcutManager* shortcut_manager =
      g_browser_process->profile_manager()->profile_shortcut_manager();
  DCHECK(shortcut_manager);

  shortcut_manager->CreateProfileShortcut(profile_->GetPath());
}

void ManageProfileHandler::HandleRemoveProfileShortcut(
    const base::ListValue* args) {
  DCHECK(ProfileShortcutManager::IsFeatureEnabled());
  ProfileShortcutManager* shortcut_manager =
    g_browser_process->profile_manager()->profile_shortcut_manager();
  DCHECK(shortcut_manager);

  shortcut_manager->RemoveProfileShortcuts(profile_->GetPath());
}

}  // namespace settings
