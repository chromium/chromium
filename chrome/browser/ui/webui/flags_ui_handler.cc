// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui_handler.h"

#include "base/bind.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/channel_info.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "components/version_info/channel.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user_manager.h"
#endif

FlagsUIHandler::FlagsUIHandler()
    : access_(flags_ui::kGeneralAccessFlagsOnly),
      experimental_features_requested_(false),
      deprecated_features_only_(false) {}

FlagsUIHandler::~FlagsUIHandler() {}

void FlagsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      flags_ui::kRequestExperimentalFeatures,
      base::BindRepeating(&FlagsUIHandler::HandleRequestExperimentalFeatures,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kEnableExperimentalFeature,
      base::BindRepeating(
          &FlagsUIHandler::HandleEnableExperimentalFeatureMessage,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kSetOriginListFlag,
      base::BindRepeating(&FlagsUIHandler::HandleSetOriginListFlagMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kRestartBrowser,
      base::BindRepeating(&FlagsUIHandler::HandleRestartBrowser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kResetAllFlags,
      base::BindRepeating(&FlagsUIHandler::HandleResetAllFlags,
                          base::Unretained(this)));
}

void FlagsUIHandler::Init(flags_ui::FlagsStorage* flags_storage,
                          flags_ui::FlagAccess access) {
  flags_storage_.reset(flags_storage);
  access_ = access;

  if (experimental_features_requested_)
    HandleRequestExperimentalFeatures(nullptr);
}

void FlagsUIHandler::HandleRequestExperimentalFeatures(
    const base::ListValue* args) {
  experimental_features_requested_ = true;
  // Bail out if the handler hasn't been initialized yet. The request will be
  // handled after the initialization.
  if (!flags_storage_)
    return;

  base::DictionaryValue results;

  std::unique_ptr<base::ListValue> supported_features(new base::ListValue);
  std::unique_ptr<base::ListValue> unsupported_features(new base::ListValue);

  if (deprecated_features_only_) {
    about_flags::GetFlagFeatureEntriesForDeprecatedPage(
        flags_storage_.get(), access_, supported_features.get(),
        unsupported_features.get());
  } else {
    about_flags::GetFlagFeatureEntries(flags_storage_.get(), access_,
                                       supported_features.get(),
                                       unsupported_features.get());
  }

  results.Set(flags_ui::kSupportedFeatures, std::move(supported_features));
  results.Set(flags_ui::kUnsupportedFeatures, std::move(unsupported_features));
  results.SetBoolean(flags_ui::kNeedsRestart,
                     about_flags::IsRestartNeededToCommitChanges());
  results.SetBoolean(flags_ui::kShowOwnerWarning,
                     access_ == flags_ui::kGeneralAccessFlagsOnly);

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  version_info::Channel channel = chrome::GetChannel();
  results.SetBoolean(
      flags_ui::kShowBetaChannelPromotion,
      channel == version_info::Channel::STABLE && !deprecated_features_only_);
  results.SetBoolean(
      flags_ui::kShowDevChannelPromotion,
      channel == version_info::Channel::BETA && !deprecated_features_only_);
#else
  results.SetBoolean(flags_ui::kShowBetaChannelPromotion, false);
  results.SetBoolean(flags_ui::kShowDevChannelPromotion, false);
#endif
  web_ui()->CallJavascriptFunctionUnsafe(flags_ui::kReturnExperimentalFeatures,
                                         results);
}

void FlagsUIHandler::HandleEnableExperimentalFeatureMessage(
    const base::ListValue* args) {
  DCHECK(flags_storage_);
  DCHECK_EQ(2u, args->GetSize());
  if (args->GetSize() != 2)
    return;

  std::string entry_internal_name;
  std::string enable_str;
  if (!args->GetString(0, &entry_internal_name) ||
      !args->GetString(1, &enable_str))
    return;

  about_flags::SetFeatureEntryEnabled(flags_storage_.get(), entry_internal_name,
                                      enable_str == "true");
}

void FlagsUIHandler::HandleSetOriginListFlagMessage(
    const base::ListValue* args) {
  DCHECK(flags_storage_);
  if (args->GetSize() != 2) {
    NOTREACHED();
    return;
  }

  std::string entry_internal_name;
  std::string value_str;
  if (!args->GetString(0, &entry_internal_name) ||
      !args->GetString(1, &value_str) || entry_internal_name.empty()) {
    NOTREACHED();
    return;
  }

  about_flags::SetOriginListFlag(entry_internal_name, value_str,
                                 flags_storage_.get());
}

void FlagsUIHandler::HandleRestartBrowser(const base::ListValue* args) {
  DCHECK(flags_storage_);
#if defined(OS_CHROMEOS)
  // On ChromeOS be less intrusive and restart inside the user session after
  // we apply the newly selected flags.
  base::CommandLine user_flags(base::CommandLine::NO_PROGRAM);
  about_flags::ConvertFlagsToSwitches(flags_storage_.get(), &user_flags,
                                      flags_ui::kAddSentinels);

  // Adhere to policy-enforced command-line switch handling when
  // applying modified flags..
  chromeos::UserSessionManager::ApplyUserPolicyToSwitches(
      Profile::FromWebUI(web_ui())->GetPrefs(), &user_flags);

  base::CommandLine::StringVector flags;
  // argv[0] is the program name |base::CommandLine::NO_PROGRAM|.
  flags.assign(user_flags.argv().begin() + 1, user_flags.argv().end());
  VLOG(1) << "Restarting to apply per-session flags...";
  AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  chromeos::UserSessionManager::GetInstance()->SetSwitchesForUser(
      account_id,
      chromeos::UserSessionManager::CommandLineSwitchesType::
          kPolicyAndFlagsAndKioskControl,
      flags);
#endif
  chrome::AttemptRestart();
}

void FlagsUIHandler::HandleResetAllFlags(const base::ListValue* args) {
  DCHECK(flags_storage_);
  about_flags::ResetAllFlags(flags_storage_.get());
}
