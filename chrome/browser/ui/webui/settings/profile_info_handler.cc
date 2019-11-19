// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/profile_info_handler.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/chromeos/user_image_source.h"
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
#if defined(OS_CHROMEOS)
  // Set up the chrome://userimage/ source.
  content::URLDataSource::Add(profile,
                              std::make_unique<chromeos::UserImageSource>());
#endif
}

ProfileInfoHandler::~ProfileInfoHandler() {}

void ProfileInfoHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfileInfo",
      base::BindRepeating(&ProfileInfoHandler::HandleGetProfileInfo,
                          base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "getProfileStatsCount",
      base::BindRepeating(&ProfileInfoHandler::HandleGetProfileStats,
                          base::Unretained(this)));
#endif
}

void ProfileInfoHandler::OnJavascriptAllowed() {
  profile_observer_.Add(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

#if defined(OS_CHROMEOS)
  user_manager_observer_.Add(user_manager::UserManager::Get());
#endif
}

void ProfileInfoHandler::OnJavascriptDisallowed() {
  callback_weak_ptr_factory_.InvalidateWeakPtrs();

  profile_observer_.Remove(
      &g_browser_process->profile_manager()->GetProfileAttributesStorage());

#if defined(OS_CHROMEOS)
  user_manager_observer_.Remove(user_manager::UserManager::Get());
#endif
}

#if defined(OS_CHROMEOS)
void ProfileInfoHandler::OnUserImageChanged(const user_manager::User& user) {
  PushProfileInfo();
}
#endif

void ProfileInfoHandler::OnProfileNameChanged(
    const base::FilePath& /* profile_path */,
    const base::string16& /* old_profile_name */) {
  PushProfileInfo();
}

void ProfileInfoHandler::OnProfileAvatarChanged(
    const base::FilePath& /* profile_path */) {
  PushProfileInfo();
}

void ProfileInfoHandler::HandleGetProfileInfo(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(*callback_id, *GetAccountNameAndIcon());
}

#if !defined(OS_CHROMEOS)
void ProfileInfoHandler::HandleGetProfileStats(const base::ListValue* args) {
  AllowJavascript();

  ProfileStatisticsFactory::GetForProfile(profile_)->GatherStatistics(
      base::Bind(&ProfileInfoHandler::PushProfileStatsCount,
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
  FireWebUIListener(kProfileInfoChangedEventName, *GetAccountNameAndIcon());
}

std::unique_ptr<base::DictionaryValue>
ProfileInfoHandler::GetAccountNameAndIcon() const {
  std::string name;
  std::string icon_url;

#if defined(OS_CHROMEOS)
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);
  name = base::UTF16ToUTF8(user->GetDisplayName());

  // Get image as data URL instead of using chrome://userimage source to avoid
  // issues with caching.
  scoped_refptr<base::RefCountedMemory> image =
      chromeos::UserImageSource::GetUserImage(user->GetAccountId());
  icon_url = webui::GetPngDataUrl(image->front(), image->size());
#else   // !defined(OS_CHROMEOS)
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    name = base::UTF16ToUTF8(
        ProfileAttributesEntry::ShouldConcatenateGaiaAndProfileName()
            ? entry->GetLocalProfileName()
            : entry->GetName());
    // TODO(crbug.com/710660): return chrome://theme/IDR_PROFILE_AVATAR_*
    // and update theme_source.cc to get high res avatar icons. This does less
    // work here, sends less over IPC, and is more stable with returned results.
    int kAvatarIconSize = 40.f * web_ui()->GetDeviceScaleFactor();
    gfx::Image icon = profiles::GetSizedAvatarIcon(
        entry->GetAvatarIcon(), true, kAvatarIconSize, kAvatarIconSize);
    icon_url = webui::GetBitmapDataUrl(icon.AsBitmap());
  }
#endif  // defined(OS_CHROMEOS)

  auto response = std::make_unique<base::DictionaryValue>();
  response->SetString("name", name);
  response->SetString("iconUrl", icon_url);
  return response;
}

}  // namespace settings
