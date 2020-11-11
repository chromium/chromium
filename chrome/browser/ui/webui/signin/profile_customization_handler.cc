// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
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
  observed_profile_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());
}

void ProfileCustomizationHandler::OnJavascriptDisallowed() {
  observed_profile_.RemoveAll();
}

void ProfileCustomizationHandler::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  if (profile_path != profile_path_)
    return;
  UpdateProfileInfo();
}

void ProfileCustomizationHandler::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  if (profile_path != profile_path_)
    return;
  UpdateProfileInfo();
}

void ProfileCustomizationHandler::OnProfileThemeColorsChanged(
    const base::FilePath& profile_path) {
  DCHECK(IsJavascriptAllowed());
  if (profile_path != profile_path_)
    return;
  UpdateProfileInfo();
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
  base::string16 profile_name =
      base::UTF8ToUTF16(args->GetList()[0].GetString());

  base::TrimWhitespace(profile_name, base::TRIM_ALL, &profile_name);
  DCHECK(!profile_name.empty());
  GetProfileEntry()->SetLocalProfileName(profile_name);

  if (done_closure_)
    std::move(done_closure_).Run();
}

void ProfileCustomizationHandler::UpdateProfileInfo() {
  DCHECK(IsJavascriptAllowed());
  FireWebUIListener("on-profile-info-changed", GetProfileInfoValue());
}

base::Value ProfileCustomizationHandler::GetProfileInfoValue() {
  ProfileAttributesEntry* entry = GetProfileEntry();
  SkColor profile_color =
      entry->GetProfileThemeColors().profile_highlight_color;

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("textColor",
                    color_utils::SkColorToRgbaString(
                        GetProfileForegroundTextColor(profile_color)));
  dict.SetStringKey("backgroundColor",
                    color_utils::SkColorToRgbaString(profile_color));

  const int avatar_icon_size = kAvatarSize * web_ui()->GetDeviceScaleFactor();
  gfx::Image icon =
      profiles::GetSizedAvatarIcon(entry->GetAvatarIcon(avatar_icon_size), true,
                                   avatar_icon_size, avatar_icon_size);
  dict.SetStringKey("pictureUrl", webui::GetBitmapDataUrl(icon.AsBitmap()));
  return dict;
}

ProfileAttributesEntry* ProfileCustomizationHandler::GetProfileEntry() const {
  ProfileAttributesEntry* entry = nullptr;
  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile_path_, &entry);
  DCHECK(entry);
  return entry;
}
