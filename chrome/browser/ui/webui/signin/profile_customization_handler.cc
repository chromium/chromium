// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "signin_url_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"

namespace {
const size_t kAvatarSize = 60;
}

ProfileCustomizationHandler::ProfileCustomizationHandler(
    Profile* profile,
    base::OnceCallback<void(CustomizationResult)> completion_callback)
    : profile_(profile), completion_callback_(std::move(completion_callback)) {}

ProfileCustomizationHandler::~ProfileCustomizationHandler() = default;

void ProfileCustomizationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialized",
      base::BindRepeating(&ProfileCustomizationHandler::HandleInitialized,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAvailableIcons",
      base::BindRepeating(&ProfileCustomizationHandler::HandleGetAvailableIcons,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "done", base::BindRepeating(&ProfileCustomizationHandler::HandleDone,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "skip", base::BindRepeating(&ProfileCustomizationHandler::HandleSkip,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteProfile",
      base::BindRepeating(&ProfileCustomizationHandler::HandleDeleteProfile,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setAvatarIcon",
      base::BindRepeating(&ProfileCustomizationHandler::HandleSetAvatarIcon,
                          base::Unretained(this)));
}

void ProfileCustomizationHandler::OnJavascriptAllowed() {
  observed_profile_.Observe(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());
}

void ProfileCustomizationHandler::OnJavascriptDisallowed() {
  observed_profile_.Reset();
}

void ProfileCustomizationHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
  FireWebUIListener(
      "on-available-icons-changed",
      profiles::GetIconsAndLabelsForProfileAvatarSelector(profile_->GetPath()));
}

void ProfileCustomizationHandler::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void ProfileCustomizationHandler::OnProfileThemeColorsChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
  FireWebUIListener(
      "on-available-icons-changed",
      profiles::GetIconsAndLabelsForProfileAvatarSelector(profile_->GetPath()));
}

void ProfileCustomizationHandler::OnProfileHostedDomainChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void ProfileCustomizationHandler::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const std::u16string& old_profile_name) {
  UpdateProfileInfo(profile_path);
}

void ProfileCustomizationHandler::HandleInitialized(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, GetProfileInfoValue());
}

void ProfileCustomizationHandler::HandleGetAvailableIcons(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      profiles::GetIconsAndLabelsForProfileAvatarSelector(profile_->GetPath()));
}

void ProfileCustomizationHandler::HandleDone(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::u16string profile_name = base::UTF8ToUTF16(args[0].GetString());

  base::TrimWhitespace(profile_name, base::TRIM_ALL, &profile_name);
  DCHECK(!profile_name.empty());

  const GURL& url = web_ui()->GetWebContents()->GetVisibleURL();
  if (GetProfileCustomizationStyle(url) ==
      ProfileCustomizationStyle::kLocalProfileCreation) {
    // The local profile is created at first as ephemeral and its creation is
    // finalized when customization is successfully completed.
    FinalizeNewProfileSetup(profile_, profile_name, /*is_default_name=*/false);
  } else {
    // TODO(crbug.com/40264199): Look into whether this branch should be also
    // covered by calling FinalizeNewProfileSetup().
    GetProfileEntry()->SetLocalProfileName(profile_name,
                                           /*is_default_name=*/false);
  }

  if (completion_callback_)
    std::move(completion_callback_).Run(CustomizationResult::kDone);
}

void ProfileCustomizationHandler::HandleSkip(const base::Value::List& args) {
  CHECK_EQ(0u, args.size());

  if (completion_callback_)
    std::move(completion_callback_).Run(CustomizationResult::kSkip);
}

void ProfileCustomizationHandler::HandleDeleteProfile(
    const base::Value::List& args) {
  CHECK_EQ(0u, args.size());

  DCHECK(GetProfileEntry()->IsEphemeral());
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion));
  // Since the profile is ephemeral, closing all browser windows triggers the
  // deletion.
  BrowserList::CloseAllBrowsersWithProfile(
      profile_, BrowserList::CloseCallback(), BrowserList::CloseCallback(),
      /*skip_beforeunload=*/true);
}

void ProfileCustomizationHandler::HandleSetAvatarIcon(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  size_t avatar_icon_index = args[0].GetInt();

  profiles::SetDefaultProfileAvatarIndex(profile_, avatar_icon_index);
}

void ProfileCustomizationHandler::UpdateProfileInfo(
    const base::FilePath& profile_path) {
  DCHECK(IsJavascriptAllowed());
  if (profile_path != profile_->GetPath())
    return;
  FireWebUIListener("on-profile-info-changed", GetProfileInfoValue());
}

base::Value::Dict ProfileCustomizationHandler::GetProfileInfoValue() {
  ProfileAttributesEntry* entry = GetProfileEntry();

  base::Value::Dict dict;
  dict.Set("backgroundColor",
           color_utils::SkColorToRgbaString(
               entry->GetProfileThemeColors().profile_highlight_color));
  const int avatar_icon_size = kAvatarSize * web_ui()->GetDeviceScaleFactor();
  gfx::Image icon =
      profiles::GetSizedAvatarIcon(entry->GetAvatarIcon(avatar_icon_size),
                                   avatar_icon_size, avatar_icon_size);
  dict.Set("pictureUrl", webui::GetBitmapDataUrl(icon.AsBitmap()));
  dict.Set("isManaged", AccountInfo::IsManaged(entry->GetHostedDomain()));
  std::u16string gaia_name = entry->GetGAIANameToDisplay();
  if (gaia_name.empty())
    gaia_name = entry->GetLocalProfileName();
  dict.Set("welcomeTitle", l10n_util::GetStringFUTF8(
                               IDS_PROFILE_CUSTOMIZATION_WELCOME, gaia_name));
  return dict;
}

ProfileAttributesEntry* ProfileCustomizationHandler::GetProfileEntry() const {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  DCHECK(entry);
  return entry;
}
