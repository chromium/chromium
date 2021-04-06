// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"

namespace {
const size_t kAvatarSize = 60;
}

ProfileCustomizationHandler::ProfileCustomizationHandler(
    base::OnceClosure done_closure)
    : done_closure_(std::move(done_closure)) {}

ProfileCustomizationHandler::~ProfileCustomizationHandler() = default;

void ProfileCustomizationHandler::RegisterMessages() {
  profile_path_ = Profile::FromWebUI(web_ui())->GetPath();
  web_ui()->RegisterMessageCallback(
      "initialized",
      base::BindRepeating(&ProfileCustomizationHandler::HandleInitialized,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "done", base::BindRepeating(&ProfileCustomizationHandler::HandleDone,
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
}

void ProfileCustomizationHandler::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
}

void ProfileCustomizationHandler::OnProfileThemeColorsChanged(
    const base::FilePath& profile_path) {
  UpdateProfileInfo(profile_path);
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
    const base::ListValue* args) {
  CHECK_EQ(1u, args->GetSize());
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];
  ResolveJavascriptCallback(callback_id, GetProfileInfoValue());
}

void ProfileCustomizationHandler::HandleDone(const base::ListValue* args) {
  CHECK_EQ(1u, args->GetSize());
  std::u16string profile_name =
      base::UTF8ToUTF16(args->GetList()[0].GetString());

  base::TrimWhitespace(profile_name, base::TRIM_ALL, &profile_name);
  DCHECK(!profile_name.empty());
  GetProfileEntry()->SetLocalProfileName(profile_name,
                                         /*is_default_name=*/false);

  if (done_closure_)
    std::move(done_closure_).Run();
}

void ProfileCustomizationHandler::UpdateProfileInfo(
    const base::FilePath& profile_path) {
  DCHECK(IsJavascriptAllowed());
  if (profile_path != profile_path_)
    return;
  FireWebUIListener("on-profile-info-changed", GetProfileInfoValue());
}

base::Value ProfileCustomizationHandler::GetProfileInfoValue() {
  ProfileAttributesEntry* entry = GetProfileEntry();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(
      "backgroundColor",
      color_utils::SkColorToRgbaString(
          entry->GetProfileThemeColors().profile_highlight_color));
  const int avatar_icon_size = kAvatarSize * web_ui()->GetDeviceScaleFactor();
  gfx::Image icon =
      profiles::GetSizedAvatarIcon(entry->GetAvatarIcon(avatar_icon_size), true,
                                   avatar_icon_size, avatar_icon_size);
  dict.SetStringKey("pictureUrl", webui::GetBitmapDataUrl(icon.AsBitmap()));
  dict.SetBoolKey("isManaged",
                  AccountInfo::IsManaged(entry->GetHostedDomain()));
  std::u16string gaia_name = entry->GetGAIANameToDisplay();
  if (gaia_name.empty())
    gaia_name = entry->GetLocalProfileName();
  dict.SetStringKey(
      "welcomeTitle",
      l10n_util::GetStringFUTF8(IDS_PROFILE_CUSTOMIZATION_WELCOME, gaia_name));
  return dict;
}

ProfileAttributesEntry* ProfileCustomizationHandler::GetProfileEntry() const {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path_);
  DCHECK(entry);
  return entry;
}
