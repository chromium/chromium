// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_create_profile_handler.h"

#include <stddef.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

SigninCreateProfileHandler::SigninCreateProfileHandler() = default;

SigninCreateProfileHandler::~SigninCreateProfileHandler() {
  BrowserList::RemoveObserver(this);
}

void SigninCreateProfileHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "createDesktopShortcutLabel",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_CREATE_DESKTOP_SHORTCUT_LABEL));
  localized_strings->SetString("createProfileConfirm",
                               l10n_util::GetStringUTF16(IDS_ADD));
  localized_strings->SetString("learnMore",
                               l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  localized_strings->SetString(
      "createProfileTitle",
      l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_TITLE));
  localized_strings->SetString(
      "createProfileNamePlaceholder",
      l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_NAME_PLACEHOLDER));
  localized_strings->SetString(
      "exitAndChildlockLabel",
      l10n_util::GetStringUTF16(
          IDS_PROFILES_PROFILE_SIGNOUT_BUTTON));
}

void SigninCreateProfileHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "createProfile",
      base::BindRepeating(&SigninCreateProfileHandler::CreateProfile,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "requestDefaultProfileIcons",
      base::BindRepeating(
          &SigninCreateProfileHandler::RequestDefaultProfileIcons,
          base::Unretained(this)));
}

void SigninCreateProfileHandler::OnBrowserAdded(Browser* browser) {
  // Only respond to one OnBrowserAdded.
  BrowserList::RemoveObserver(this);
  UserManager::Hide();
}

void SigninCreateProfileHandler::OpenNewWindowForProfile(
    Profile* profile,
    Profile::CreateStatus status) {
  profiles::OpenBrowserWindowForProfile(
      base::Bind(&SigninCreateProfileHandler::OnBrowserReadyCallback,
                 weak_ptr_factory_.GetWeakPtr()),
      false,  // Don't create a window if one already exists.
      true,   // Create a first run window.
      false,  // There is no need to unblock all extensions because we only open
              // browser window if the Profile is not locked. Hence there is no
              // extension blocked.
      profile, status);
}

void SigninCreateProfileHandler::OpenForceSigninDialogForProfile(
    Profile* profile) {
  UserManagerProfileDialog::ShowForceSigninDialog(
      web_ui()->GetWebContents()->GetBrowserContext(), profile->GetPath());
}

void SigninCreateProfileHandler::DoCreateProfile(const base::string16& name,
                                                 const std::string& icon_url,
                                                 bool create_shortcut) {
  ProfileMetrics::LogProfileAddNewUser(ProfileMetrics::ADD_NEW_USER_DIALOG);

  profile_path_being_created_ = ProfileManager::CreateMultiProfileAsync(
      name, icon_url,
      base::Bind(&SigninCreateProfileHandler::OnProfileCreated,
                 weak_ptr_factory_.GetWeakPtr(), create_shortcut));
}

void SigninCreateProfileHandler::RequestDefaultProfileIcons(
    const base::ListValue* args) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback", base::Value("profile-icons-received"),
      *profiles::GetDefaultProfileAvatarIconsAndLabels());
}

void SigninCreateProfileHandler::CreateProfile(const base::ListValue* args) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // We can have only one in progress profile creation
  // at any given moment, if new ones are initiated just
  // ignore them until we are done with the old one.
  if (profile_creation_type_ != NO_CREATION_IN_PROGRESS)
    return;

  profile_creation_type_ = NON_SUPERVISED_PROFILE_CREATION;

  DCHECK(profile_path_being_created_.empty());
  profile_creation_start_time_ = base::TimeTicks::Now();

  base::string16 name;
  std::string icon_url;
  bool create_shortcut = false;
  if (args->GetString(0, &name) && args->GetString(1, &icon_url)) {
    DCHECK(base::IsStringASCII(icon_url));
    base::TrimWhitespace(name, base::TRIM_ALL, &name);
    CHECK(!name.empty());
#ifndef NDEBUG
    size_t icon_index;
    DCHECK(profiles::IsDefaultAvatarIconUrl(icon_url, &icon_index));
#endif
    args->GetBoolean(2, &create_shortcut);
  }
  DoCreateProfile(name, icon_url, create_shortcut);
}

void SigninCreateProfileHandler::OnProfileCreated(
    bool create_shortcut,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_CREATED)
    RecordProfileCreationMetrics(status);

  switch (status) {
    case Profile::CREATE_STATUS_LOCAL_FAIL: {
      ShowProfileCreationError(profile, GetProfileCreationErrorMessageLocal());
      break;
    }
    case Profile::CREATE_STATUS_CREATED: {
      // Do nothing for an intermediate status.
      break;
    }
    case Profile::CREATE_STATUS_INITIALIZED: {
      HandleProfileCreationSuccess(create_shortcut, profile);
      break;
    }
    // User-initiated cancellation is handled in CancelProfileRegistration and
    // does not call this callback.
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::MAX_CREATE_STATUS: {
      NOTREACHED();
      break;
    }
  }
}

void SigninCreateProfileHandler::HandleProfileCreationSuccess(
    bool create_shortcut,
    Profile* profile) {
  switch (profile_creation_type_) {
    case NON_SUPERVISED_PROFILE_CREATION: {
      CreateShortcutAndShowSuccess(create_shortcut, profile);
      break;
    }
    case NO_CREATION_IN_PROGRESS:
      NOTREACHED();
      break;
  }
}

void SigninCreateProfileHandler::CreateShortcutAndShowSuccess(
    bool create_shortcut,
    Profile* profile) {
  if (create_shortcut) {
    DCHECK(ProfileShortcutManager::IsFeatureEnabled());
    ProfileShortcutManager* shortcut_manager =
        g_browser_process->profile_manager()->profile_shortcut_manager();
    DCHECK(shortcut_manager);
    if (shortcut_manager)
      shortcut_manager->CreateProfileShortcut(profile->GetPath());
  }

  DCHECK_EQ(profile_path_being_created_.value(), profile->GetPath().value());
  profile_path_being_created_.clear();
  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);

  bool is_force_signin_enabled = signin_util::IsForceSigninEnabled();
  bool open_new_window = !is_force_signin_enabled;

  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback",
      GetWebUIListenerName(PROFILE_CREATION_SUCCESS));

  if (open_new_window) {
    // Opening the new window must be the last action, after all callbacks
    // have been run, to give them a chance to initialize the profile.
    OpenNewWindowForProfile(profile, Profile::CREATE_STATUS_INITIALIZED);
  } else if (is_force_signin_enabled) {
    OpenForceSigninDialogForProfile(profile);
  }
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
}

void SigninCreateProfileHandler::ShowProfileCreationError(
    Profile* profile,
    const base::string16& error) {
  DCHECK_NE(NO_CREATION_IN_PROGRESS, profile_creation_type_);
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.webUIListenerCallback", GetWebUIListenerName(PROFILE_CREATION_ERROR),
      base::Value(error));
  // The ProfileManager calls us back with a NULL profile in some cases.
  if (profile) {
    webui::DeleteProfileAtPath(profile->GetPath(),
                               ProfileMetrics::DELETE_PROFILE_SETTINGS);
  }
  profile_creation_type_ = NO_CREATION_IN_PROGRESS;
  profile_path_being_created_.clear();
}

void SigninCreateProfileHandler::RecordProfileCreationMetrics(
    Profile::CreateStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Profile.CreateResult", status,
                            Profile::MAX_CREATE_STATUS);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Profile.CreateTimeNoTimeout",
      base::TimeTicks::Now() - profile_creation_start_time_);
}

base::string16 SigninCreateProfileHandler::GetProfileCreationErrorMessageLocal()
    const {
  return l10n_util::GetStringUTF16(IDS_PROFILES_CREATE_LOCAL_ERROR);
}

void SigninCreateProfileHandler::OnBrowserReadyCallback(
    Profile* profile,
    Profile::CreateStatus profile_create_status) {
  Browser* browser = chrome::FindAnyBrowser(profile, false);
  // Closing the User Manager before the window is created can flakily cause
  // Chrome to close.
  if (browser && browser->window())
    UserManager::Hide();
  else
    BrowserList::AddObserver(this);
}

base::Value SigninCreateProfileHandler::GetWebUIListenerName(
    ProfileCreationStatus status) const {
  switch (status) {
    case PROFILE_CREATION_SUCCESS:
      return base::Value("create-profile-success");
    case PROFILE_CREATION_ERROR:
      return base::Value("create-profile-error");
  }
  NOTREACHED();
  return base::Value(std::string());
}
