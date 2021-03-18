// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags/flags_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/flags/flags_ui_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/owner_flags_storage.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateFlagsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFlagsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");
  source->AddString(flags_ui::kVersion, version_info::GetVersionNumber());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!user_manager::UserManager::Get()->IsCurrentUserOwner() &&
      base::SysInfo::IsRunningOnChromeOS()) {
    // Set the string to show which user can actually change the flags.
    std::string owner;
    ash::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
    source->AddString("owner-warning",
                      l10n_util::GetStringFUTF16(IDS_FLAGS_UI_OWNER_WARNING,
                                                 base::UTF8ToUTF16(owner)));
  } else {
    source->AddString("owner-warning", std::u16string());
  }
#endif

  source->AddResourcePath(flags_ui::kFlagsJS, IDR_FLAGS_UI_FLAGS_JS);
  source->AddResourcePath(flags_ui::kFlagsCSS, IDR_FLAGS_UI_FLAGS_CSS);
  source->SetDefaultResource(IDR_FLAGS_UI_FLAGS_HTML);
  source->UseStringsJs();
  return source;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// On ChromeOS verifying if the owner is signed in is async operation and only
// after finishing it the UI can be properly populated. This function is the
// callback for whether the owner is signed in. It will respectively pick the
// proper PrefService for the flags interface.
template <class T>
void FinishInitialization(base::WeakPtr<T> flags_ui,
                          Profile* profile,
                          FlagsUIHandler* dom_handler,
                          bool current_user_is_owner) {
  DCHECK(!profile->IsOffTheRecord());
  // If the flags_ui has gone away, there's nothing to do.
  if (!flags_ui)
    return;

  // On Chrome OS the owner can set system wide flags and other users can only
  // set flags for their own session.
  // Note that |dom_handler| is owned by the web ui that owns |flags_ui|, so
  // it is still alive if |flags_ui| is.
  if (current_user_is_owner) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
    dom_handler->Init(new chromeos::about_flags::OwnerFlagsStorage(
                          profile->GetPrefs(), service),
                      flags_ui::kOwnerAccessToFlags);
  } else {
    dom_handler->Init(
        new flags_ui::PrefServiceFlagsStorage(profile->GetPrefs()),
        flags_ui::kGeneralAccessFlagsOnly);
  }

  // Show a warning info bar when kSafeMode switch is present.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSafeMode)) {
    SimpleAlertInfoBarDelegate::Create(
        InfoBarService::FromWebContents(flags_ui->web_ui()->GetWebContents()),
        infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE,
        &vector_icons::kWarningIcon,
        l10n_util::GetStringUTF16(IDS_FLAGS_IGNORED_DUE_TO_CRASHY_CHROME),
        /*auto_expire=*/false, /*should_animate=*/true);
  }

  // Show a warning info bar for secondary users.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    SimpleAlertInfoBarDelegate::Create(
        InfoBarService::FromWebContents(flags_ui->web_ui()->GetWebContents()),
        infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE,
        &vector_icons::kWarningIcon,
        l10n_util::GetStringUTF16(IDS_FLAGS_IGNORED_SECONDARY_USERS),
        /*auto_expire=*/false, /*should_animate=*/true);
  }
}
#endif

}  // namespace

// static
void FlagsUI::AddStrings(content::WebUIDataSource* source) {
  // Strings added here are all marked a non-translatable, so they are not
  // actually localized.
  source->AddLocalizedString(flags_ui::kFlagsRestartNotice,
                             IDS_FLAGS_UI_RELAUNCH_NOTICE);
  source->AddLocalizedString("available", IDS_FLAGS_UI_AVAILABLE_FEATURE);
  source->AddLocalizedString("clear-search", IDS_FLAGS_UI_CLEAR_SEARCH);
  source->AddLocalizedString("disabled", IDS_FLAGS_UI_DISABLED_FEATURE);
  source->AddLocalizedString("enabled", IDS_FLAGS_UI_ENABLED_FEATURE);
  source->AddLocalizedString("experiment-enabled",
                             IDS_FLAGS_UI_EXPERIMENT_ENABLED);
  source->AddLocalizedString("heading", IDS_FLAGS_UI_TITLE);
  source->AddLocalizedString("no-results", IDS_FLAGS_UI_NO_RESULTS);
  source->AddLocalizedString("not-available-platform",
                             IDS_FLAGS_UI_NOT_AVAILABLE_ON_PLATFORM);
  source->AddLocalizedString("page-warning", IDS_FLAGS_UI_PAGE_WARNING);
  source->AddLocalizedString("page-warning-explanation",
                             IDS_FLAGS_UI_PAGE_WARNING_EXPLANATION);
  source->AddLocalizedString("relaunch", IDS_FLAGS_UI_RELAUNCH);
  source->AddLocalizedString("reset", IDS_FLAGS_UI_PAGE_RESET);
  source->AddLocalizedString("reset-acknowledged",
                             IDS_FLAGS_UI_RESET_ACKNOWLEDGED);
  source->AddLocalizedString("search-label", IDS_FLAGS_UI_SEARCH_LABEL);
  source->AddLocalizedString("search-placeholder",
                             IDS_FLAGS_UI_SEARCH_PLACEHOLDER);
  source->AddLocalizedString("title", IDS_FLAGS_UI_TITLE);
  source->AddLocalizedString("unavailable", IDS_FLAGS_UI_UNAVAILABLE_FEATURE);
  source->AddLocalizedString("searchResultsSingular",
                             IDS_FLAGS_UI_SEARCH_RESULTS_SINGULAR);
  source->AddLocalizedString("searchResultsPlural",
                             IDS_FLAGS_UI_SEARCH_RESULTS_PLURAL);
}

// static
void FlagsDeprecatedUI::AddStrings(content::WebUIDataSource* source) {
  source->AddLocalizedString(flags_ui::kFlagsRestartNotice,
                             IDS_DEPRECATED_FEATURES_RELAUNCH_NOTICE);
  source->AddLocalizedString("available",
                             IDS_DEPRECATED_FEATURES_AVAILABLE_FEATURE);
  source->AddLocalizedString("clear-search", IDS_DEPRECATED_UI_CLEAR_SEARCH);
  source->AddLocalizedString("disabled",
                             IDS_DEPRECATED_FEATURES_DISABLED_FEATURE);
  source->AddLocalizedString("enabled",
                             IDS_DEPRECATED_FEATURES_ENABLED_FEATURE);
  source->AddLocalizedString("experiment-enabled",
                             IDS_DEPRECATED_UI_EXPERIMENT_ENABLED);
  source->AddLocalizedString("heading", IDS_DEPRECATED_FEATURES_HEADING);
  source->AddLocalizedString("no-results", IDS_DEPRECATED_FEATURES_NO_RESULTS);
  source->AddLocalizedString("not-available-platform",
                             IDS_DEPRECATED_FEATURES_NOT_AVAILABLE_ON_PLATFORM);
  source->AddString("page-warning", std::string());
  source->AddLocalizedString("page-warning-explanation",
                             IDS_DEPRECATED_FEATURES_PAGE_WARNING_EXPLANATION);
  source->AddLocalizedString("relaunch", IDS_DEPRECATED_FEATURES_RELAUNCH);
  source->AddLocalizedString("reset", IDS_DEPRECATED_FEATURES_PAGE_RESET);
  source->AddLocalizedString("reset-acknowledged",
                             IDS_DEPRECATED_UI_RESET_ACKNOWLEDGED);
  source->AddLocalizedString("search-label", IDS_FLAGS_UI_SEARCH_LABEL);
  source->AddLocalizedString("search-placeholder",
                             IDS_DEPRECATED_FEATURES_SEARCH_PLACEHOLDER);
  source->AddLocalizedString("title", IDS_DEPRECATED_FEATURES_TITLE);
  source->AddLocalizedString("unavailable",
                             IDS_DEPRECATED_FEATURES_UNAVAILABLE_FEATURE);
  source->AddLocalizedString("searchResultsSingular",
                             IDS_ENTERPRISE_UI_SEARCH_RESULTS_SINGULAR);
  source->AddLocalizedString("searchResultsPlural",
                             IDS_ENTERPRISE_UI_SEARCH_RESULTS_PLURAL);
}

template <class T>
FlagsUIHandler* InitializeHandler(content::WebUI* web_ui,
                                  Profile* profile,
                                  base::WeakPtrFactory<T>& weak_factory) {
  auto handler_owner = std::make_unique<FlagsUIHandler>();
  FlagsUIHandler* handler = handler_owner.get();
  web_ui->AddMessageHandler(std::move(handler_owner));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bypass possible incognito profile.
  Profile* original_profile = profile->GetOriginalProfile();
  if (base::SysInfo::IsRunningOnChromeOS() &&
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
          original_profile)) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile);
    service->IsOwnerAsync(base::BindOnce(&FinishInitialization<T>,
                                         weak_factory.GetWeakPtr(),
                                         original_profile, handler));
  } else {
    FinishInitialization(weak_factory.GetWeakPtr(), original_profile, handler,
                         false /* current_user_is_owner */);
  }
#else
  handler->Init(
      new flags_ui::PrefServiceFlagsStorage(g_browser_process->local_state()),
      flags_ui::kOwnerAccessToFlags);
#endif
  return handler;
}

FlagsUI::FlagsUI(content::WebUI* web_ui)
    : WebUIController(web_ui), weak_factory_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* handler = InitializeHandler(web_ui, profile, weak_factory_);
  DCHECK(handler);
  handler->set_deprecated_features_only(false);

  // Set up the about:flags source.
  auto* source = CreateFlagsUIHTMLSource();
  AddStrings(source);
  content::WebUIDataSource::Add(profile, source);
}

FlagsUI::~FlagsUI() {}

// static
base::RefCountedMemory* FlagsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_FLAGS_FAVICON, scale_factor);
}

FlagsDeprecatedUI::FlagsDeprecatedUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* handler = InitializeHandler(web_ui, profile, weak_factory_);
  DCHECK(handler);
  handler->set_deprecated_features_only(true);

  // Set up the about:flags/deprecated source.
  auto* source = CreateFlagsUIHTMLSource();
  AddStrings(source);
  content::WebUIDataSource::Add(profile, source);
}

FlagsDeprecatedUI::~FlagsDeprecatedUI() {}

// static
bool FlagsDeprecatedUI::IsDeprecatedUrl(const GURL& url) {
  return url.path() == "/deprecated" || url.path() == "/deprecated/";
}
