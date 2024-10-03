// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags/flags_ui_handler.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/channel_info.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "components/version_info/channel.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#endif

namespace {
bool ExtractKeyValue(const base::Value::List& args,
                     std::string& key,
                     std::string& value) {
  if (args.size() != 2) {
    return false;
  }

  if (!args[0].is_string() || !args[1].is_string()) {
    return false;
  }
  key = args[0].GetString();
  value = args[1].GetString();
  return !key.empty();
}
}  // namespace

FlagsUIHandler::FlagsUIHandler()
    : access_(flags_ui::kGeneralAccessFlagsOnly),
      experimental_features_callback_id_(""),
      deprecated_features_callback_id_("") {}

FlagsUIHandler::~FlagsUIHandler() {}

void FlagsUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestDeprecatedFeatures",
      base::BindRepeating(&FlagsUIHandler::HandleRequestDeprecatedFeatures,
                          base::Unretained(this)));
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
      flags_ui::kSetStringFlag,
      base::BindRepeating(&FlagsUIHandler::HandleSetStringFlagMessage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kRestartBrowser,
      base::BindRepeating(&FlagsUIHandler::HandleRestartBrowser,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kResetAllFlags,
      base::BindRepeating(&FlagsUIHandler::HandleResetAllFlags,
                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      flags_ui::kCrosUrlFlagsRedirect,
      base::BindRepeating(&FlagsUIHandler::HandleCrosUrlFlagsRedirect,
                          base::Unretained(this)));
#endif
}

void FlagsUIHandler::Init(std::unique_ptr<flags_ui::FlagsStorage> flags_storage,
                          flags_ui::FlagAccess access) {
  flags_storage_ = std::move(flags_storage);
  access_ = access;

  if (!experimental_features_callback_id_.empty()) {
    SendExperimentalFeatures(/*deprecated_features_only=*/false);
  }

  if (!deprecated_features_callback_id_.empty()) {
    SendExperimentalFeatures(/*deprecated_features_only=*/true);
  }
}

void FlagsUIHandler::HandleRequestDeprecatedFeatures(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  deprecated_features_callback_id_ = callback_id.GetString();
  // Bail out if the handler hasn't been initialized yet. The request will be
  // handled after the initialization.
  if (!flags_storage_) {
    return;
  }

  SendExperimentalFeatures(/*deprecated_features_only=*/true);
}

void FlagsUIHandler::HandleRequestExperimentalFeatures(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  experimental_features_callback_id_ = callback_id.GetString();
  // Bail out if the handler hasn't been initialized yet. The request will be
  // handled after the initialization.
  if (!flags_storage_) {
    return;
  }

  SendExperimentalFeatures(/*deprecated_features_only=*/false);
}

void FlagsUIHandler::SendExperimentalFeatures(bool deprecated_features_only) {
  base::Value::Dict results;

  base::Value::List supported_features;
  base::Value::List unsupported_features;

  if (deprecated_features_only) {
    about_flags::GetFlagFeatureEntriesForDeprecatedPage(
        flags_storage_.get(), access_, supported_features,
        unsupported_features);
  } else {
    about_flags::GetFlagFeatureEntries(flags_storage_.get(), access_,
                                       supported_features,
                                       unsupported_features);
  }

  results.Set(flags_ui::kSupportedFeatures, std::move(supported_features));
  results.Set(flags_ui::kUnsupportedFeatures, std::move(unsupported_features));
  results.Set(flags_ui::kNeedsRestart,
              about_flags::IsRestartNeededToCommitChanges());
  results.Set(flags_ui::kShowOwnerWarning,
              access_ == flags_ui::kGeneralAccessFlagsOnly);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool show_system_flags_link = crosapi::browser_util::IsLacrosEnabled();
#else
  const bool show_system_flags_link = true;
#endif
  results.Set(flags_ui::kShowSystemFlagsLink, show_system_flags_link);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  version_info::Channel channel = chrome::GetChannel();
  results.Set(
      flags_ui::kShowBetaChannelPromotion,
      channel == version_info::Channel::STABLE && !deprecated_features_only);
  results.Set(
      flags_ui::kShowDevChannelPromotion,
      channel == version_info::Channel::BETA && !deprecated_features_only);
#else
  results.Set(flags_ui::kShowBetaChannelPromotion, false);
  results.Set(flags_ui::kShowDevChannelPromotion, false);
#endif
  if (deprecated_features_only) {
    ResolveJavascriptCallback(base::Value(deprecated_features_callback_id_),
                              results);
    deprecated_features_callback_id_.clear();
  } else {
    ResolveJavascriptCallback(base::Value(experimental_features_callback_id_),
                              results);
    experimental_features_callback_id_.clear();
  }
}

void FlagsUIHandler::HandleEnableExperimentalFeatureMessage(
    const base::Value::List& args) {
  DCHECK(flags_storage_);
  DCHECK_EQ(2u, args.size());
  if (args.size() != 2)
    return;

  if (!args[0].is_string() || !args[1].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  const std::string& entry_internal_name = args[0].GetString();
  const std::string& enable_str = args[1].GetString();
  if (entry_internal_name.empty()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  about_flags::SetFeatureEntryEnabled(flags_storage_.get(), entry_internal_name,
                                      enable_str == "true");
}

void FlagsUIHandler::HandleSetOriginListFlagMessage(
    const base::Value::List& args) {
  DCHECK(flags_storage_);
  std::string entry_internal_name, value_str;
  if (!ExtractKeyValue(args, entry_internal_name, value_str)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  about_flags::SetOriginListFlag(entry_internal_name, value_str,
                                 flags_storage_.get());
}

void FlagsUIHandler::HandleSetStringFlagMessage(const base::Value::List& args) {
  DCHECK(flags_storage_);
  std::string entry_internal_name, value_str;
  if (!ExtractKeyValue(args, entry_internal_name, value_str)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  about_flags::SetStringFlag(entry_internal_name, value_str,
                             flags_storage_.get());
}

void FlagsUIHandler::HandleRestartBrowser(const base::Value::List& args) {
  DCHECK(flags_storage_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS be less intrusive and restart inside the user session after
  // we apply the newly selected flags.
  VLOG(1) << "Restarting to apply per-session flags...";
  ash::about_flags::FeatureFlagsUpdate(*flags_storage_,
                                       Profile::FromWebUI(web_ui())->GetPrefs())
      .UpdateSessionManager();
#endif
  chrome::AttemptRestart();
}

void FlagsUIHandler::HandleResetAllFlags(const base::Value::List& args) {
  DCHECK(flags_storage_);
  about_flags::ResetAllFlags(flags_storage_.get());
}

#if BUILDFLAG(IS_CHROMEOS)
void FlagsUIHandler::HandleCrosUrlFlagsRedirect(const base::Value::List& args) {
  about_flags::CrosUrlFlagsRedirect();
}
#endif
