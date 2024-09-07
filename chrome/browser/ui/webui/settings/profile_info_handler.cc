// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/profile_info_handler.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/user_image/user_image_source.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif

namespace settings {

// static
const char ProfileInfoHandler::kProfileInfoChangedEventName[] =
    "profile-info-changed";
const char ProfileInfoHandler::kProfileStatsCountReadyEventName[] =
    "profile-stats-count-ready";

ProfileInfoHandler::ProfileInfoHandler(Profile* profile) : profile_(profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Set up the chrome://userimage/ source.
  content::URLDataSource::Add(profile,
                              std::make_unique<ash::UserImageSource>());
#endif
}

ProfileInfoHandler::~ProfileInfoHandler() {}

void ProfileInfoHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfileInfo",
      base::BindRepeating(&ProfileInfoHandler::HandleGetProfileInfo,
                          base::Unretained(this)));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "getProfileStatsCount",
      base::BindRepeating(&ProfileInfoHandler::HandleGetProfileStats,
                          base::Unretained(this)));
#endif
}

void ProfileInfoHandler::OnJavascriptAllowed() {
  profile_observation_.Observe(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager_observation_.Observe(user_manager::UserManager::Get());
#endif
}

void ProfileInfoHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();

  DCHECK(profile_observation_.IsObservingSource(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage()));
  profile_observation_.Reset();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(user_manager_observation_.IsObservingSource(
      user_manager::UserManager::Get()));
  user_manager_observation_.Reset();
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileInfoHandler::OnUserImageChanged(const user_manager::User& user) {
  PushProfileInfo();
}
#endif

void ProfileInfoHandler::OnProfileNameChanged(
    const base::FilePath& /* profile_path */,
    const std::u16string& /* old_profile_name */) {
  PushProfileInfo();
}

void ProfileInfoHandler::OnProfileAvatarChanged(
    const base::FilePath& /* profile_path */) {
  PushProfileInfo();
}

void ProfileInfoHandler::HandleGetProfileInfo(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetAccountNameAndIcon());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileInfoHandler::HandleGetProfileStats(const base::Value::List& args) {
  AllowJavascript();

  ProfileStatisticsFactory::GetForProfile(profile_)->GatherStatistics(
      base::BindRepeating(&ProfileInfoHandler::PushProfileStatsCount,
                          callback_weak_ptr_factory_.GetWeakPtr()));
}

void ProfileInfoHandler::PushProfileStatsCount(
    profiles::ProfileCategoryStats stats) {
  int count = 0;
  for (const auto& item : stats) {
    count += item.count;
  }
  // PushProfileStatsCount gets invoked multiple times as each stat becomes
  // available. Therefore, webUIListenerCallback mechanism is used instead of
  // the Promise callback approach.
  FireWebUIListener(kProfileStatsCountReadyEventName, base::Value(count));
}
#endif

void ProfileInfoHandler::PushProfileInfo() {
  FireWebUIListener(kProfileInfoChangedEventName, GetAccountNameAndIcon());
}

base::Value::Dict ProfileInfoHandler::GetAccountNameAndIcon() {
  std::string name;
  std::string icon_url;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  name = base::UTF16ToUTF8(user->GetDisplayName());

  // Get image as data URL instead of using chrome://userimage source to avoid
  // issues with caching.
  scoped_refptr<base::RefCountedMemory> image =
      ash::UserImageSource::GetUserImage(user->GetAccountId());
  icon_url = webui::GetPngDataUrl(image->front(), image->size());
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  if (entry) {
    name = base::UTF16ToUTF8(entry->GetLocalProfileName());
    // TODO(crbug.com/40515148): return chrome://theme/IDR_PROFILE_AVATAR_*
    // and update theme_source.cc to get high res avatar icons. This does less
    // work here, sends less over IPC, and is more stable with returned results.
    int kAvatarIconSize = 40.f * web_ui()->GetDeviceScaleFactor();
    gfx::Image icon = profiles::GetSizedAvatarIcon(
        entry->GetAvatarIcon(), kAvatarIconSize, kAvatarIconSize);
    icon_url = webui::GetBitmapDataUrl(icon.AsBitmap());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::Value::Dict response;
  response.Set("name", name);
  response.Set("iconUrl", icon_url);
  return response;
}

}  // namespace settings
