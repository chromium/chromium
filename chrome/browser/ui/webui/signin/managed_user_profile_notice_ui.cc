// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/webui/webui_util.h"

ManagedUserProfileNoticeUI::ManagedUserProfileNoticeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIManagedUserProfileNoticeHost);

  static constexpr webui::ResourcePath kResources[] = {
      {"icons.html.js", IDR_SIGNIN_ICONS_HTML_JS},
      {"managed_user_profile_notice_app.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_APP_JS},
      {"managed_user_profile_notice_app.css.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_APP_CSS_JS},
      {"managed_user_profile_notice_app.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_APP_HTML_JS},
      {"managed_user_profile_notice_disclosure.css.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DISCLOSURE_CSS_JS},
      {"managed_user_profile_notice_disclosure.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DISCLOSURE_HTML_JS},
      {"managed_user_profile_notice_disclosure.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DISCLOSURE_JS},
      {"managed_user_profile_notice_state.css.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_STATE_CSS_JS},
      {"managed_user_profile_notice_value_prop.css.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_VALUE_PROP_CSS_JS},
      {"managed_user_profile_notice_data_handling.css.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DATA_HANDLING_CSS_JS},
      {"managed_user_profile_notice_data_handling.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DATA_HANDLING_HTML_JS},
      {"managed_user_profile_notice_data_handling.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_DATA_HANDLING_JS},
      {"managed_user_profile_notice_value_prop.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_VALUE_PROP_HTML_JS},
      {"managed_user_profile_notice_value_prop.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_VALUE_PROP_JS},
      {"managed_user_profile_notice_state.html.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_STATE_HTML_JS},
      {"managed_user_profile_notice_state.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_STATE_JS},
      {"managed_user_profile_notice_browser_proxy.js",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_BROWSER_PROXY_JS},
      {"images/data_handling.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_DATA_HANDLING_SVG},
      {"images/enrollment_success.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_SUCCESS_SVG},
      {"images/enrollment_failure.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_FAILURE_SVG},
      {"images/enrollment_timeout.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_TIMEOUT_SVG},
      {"images/enrollment_success_dark.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_SUCCESS_DARK_SVG},
      {"images/enrollment_failure_dark.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_FAILURE_DARK_SVG},
      {"images/enrollment_timeout_dark.svg",
       IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_IMAGES_ENROLLMENT_TIMEOUT_DARK_SVG},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
      {"tangible_sync_style_shared.css.js",
       IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_CSS_JS},
      {"tangible_sync_style_shared.css.js",
       IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_CSS_JS},
  };

  webui::SetupWebUIDataSource(
      source, kResources,
      IDR_SIGNIN_MANAGED_USER_PROFILE_NOTICE_MANAGED_USER_PROFILE_NOTICE_HTML);

  source->AddResourcePath("images/left-banner.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/left-banner-dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/right-banner.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/right-banner-dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);
  source->AddResourcePath("images/dialog_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_DIALOG_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "images/dialog_illustration_dark.svg",
      IDR_SIGNIN_IMAGES_SHARED_DIALOG_ILLUSTRATION_DARK_SVG);
  source->AddLocalizedString("enterpriseProfileWelcomeTitle",
                             IDS_ENTERPRISE_PROFILE_WELCOME_TITLE);
  source->AddLocalizedString("cancelLabel", IDS_CANCEL);
  source->AddLocalizedString("backLabel", IDS_ENTERPRISE_PROFILE_WELCOME_BACK);
  source->AddLocalizedString(
      "cancelValueProp",
      IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_DECLINE_TEXT);
  source->AddLocalizedString("continueLabel", IDS_APP_CONTINUE);
  source->AddLocalizedString("confirmLabel", IDS_CONFIRM);
  source->AddLocalizedString("closeLabel", IDS_CLOSE);
  source->AddLocalizedString("retryLabel",
                             IDS_ENTERPRISE_OIDC_WELCOME_TIMEOUT_RETRY_LABEL);
  source->AddLocalizedString("linkDataText",
                             IDS_ENTERPRISE_PROFILE_WELCOME_LINK_DATA_CHECKBOX);

  source->AddLocalizedString(
      "profileDisclosureTitle",
      IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_WORK_TITLE);
  source->AddLocalizedString(
      "profileDisclosureSubtitle",
      IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_SUBTITLE);

  source->AddLocalizedString("profileInformationTitle",
                             IDS_ENTERPRISE_WELCOME_PROFILE_INFORMATION_TITLE);
  source->AddLocalizedString(
      "profileInformationDetails",
      IDS_ENTERPRISE_WELCOME_PROFILE_INFORMATION_DETAILS);
  source->AddLocalizedString("deviceInformationTitle",
                             IDS_ENTERPRISE_WELCOME_DEVICE_INFORMATION_TITLE);
  source->AddLocalizedString("deviceInformationDetails",
                             IDS_ENTERPRISE_WELCOME_DEVICE_INFORMATION_DETAILS);

  source->AddLocalizedString("processingSubtitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_PROCESSING_SUBTITLE);
  source->AddLocalizedString(
      "longProcessingSubtitle",
      IDS_ENTERPRISE_OIDC_WELCOME_LONG_PROCESSING_SUBTITLE);

  source->AddLocalizedString("successTitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_SUCCESS_TITLE);
  source->AddLocalizedString("successSubtitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_SUCCESS_SUBTITLE);
  source->AddLocalizedString("timeoutTitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_TIMEOUT_TITLE);
  source->AddLocalizedString("timeoutSubtitle",
                             IDS_ENTERPRISE_OIDC_WELCOME_TIMEOUT_SUBTITLE);
  source->AddLocalizedString(
      "separateBrowsingDataTitle",
      IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_WORK_TITLE);
  source->AddLocalizedString(
      "valuePropTitle",
      IDS_ENTERPRISE_VALUE_PROPOSITION_PROFILE_SUGGESTED_TITLE);
  source->AddLocalizedString("valuePropSubtitle",
                             IDS_ENTERPRISE_VALUE_PROPOSITION_WORK_SUBTITLE);

  source->AddLocalizedString(
      "separateBrowsingDataChoiceTitle",
      IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_DATA_WORK_CHOICE);
  source->AddLocalizedString(
      "separateBrowsingDataChoiceDetails",
      IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_DATA_CHOICE_DETAILS);
  source->AddLocalizedString(
      "mergeBrowsingDataChoiceTitle",
      IDS_ENTERPRISE_WELCOME_MERGE_BROWSING_DATA_WORK_CHOICE);
  source->AddLocalizedString(
      "mergeBrowsingDataChoiceDetails",
      IDS_ENTERPRISE_WELCOME_MERGE_BROWSING_DATA_CHOICE_DETAILS);
  source->AddBoolean("showLinkDataCheckbox", false);
  source->AddBoolean("isModalDialog", false);
  source->AddBoolean("enforcedByPolicy", false);
  source->AddInteger("initialState",
                     ManagedUserProfileNoticeHandler::State::kDisclosure);
}

ManagedUserProfileNoticeUI::~ManagedUserProfileNoticeUI() = default;

void ManagedUserProfileNoticeUI::Initialize(
    Browser* browser,
    ManagedUserProfileNoticeUI::ScreenType type,
    std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
        create_param) {
  auto* profile = Profile::FromWebUI(web_ui());
  bool is_school_account =
      create_param->account_info.capabilities.can_use_edu_features() ==
      signin::Tribool::kTrue;
  base::Value::Dict update_data;
  std::string domain =
      enterprise_util::GetDomainFromEmail(create_param->account_info.email);
  if (type ==
      ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation) {
    update_data.Set("isModalDialog", true);

    int title_id = create_param->profile_creation_required_by_policy
                       ? IDS_ENTERPRISE_WELCOME_PROFILE_REQUIRED_TITLE
                       : IDS_ENTERPRISE_WELCOME_PROFILE_WILL_BE_MANAGED_TITLE;
    if (create_param->profile_creation_required_by_policy) {
      std::string manager =
          signin_util::IsProfileSeparationEnforcedByProfile(
              profile, create_param->account_info.email)
              ? GetEnterpriseAccountDomain(*profile).value_or(std::string())
              : domain;
      update_data.Set(
          "valuePropTitle",
          manager.empty()
              ? l10n_util::GetStringUTF16(
                    IDS_ENTERPRISE_VALUE_PROPOSITION_PROFILE_REQUIRED_BY_ORG_TITLE)
              : l10n_util::GetStringFUTF16(
                    IDS_ENTERPRISE_VALUE_PROPOSITION_PROFILE_REQUIRED_BY_ORG_KNOWN_DOMAIN_TITLE,
                    base::UTF8ToUTF16(manager)));
    }
    update_data.Set("enterpriseProfileWelcomeTitle",
                    l10n_util::GetStringUTF16(title_id));

    update_data.Set("showLinkDataCheckbox",
                    create_param->show_link_data_option);
    // If the user is already signed in and is trying to turn sync on, we can
    // skip the value proposition screen since they are already signed in.
    if (create_param->user_already_signed_in) {
      update_data.Set("initialState",
                      ManagedUserProfileNoticeHandler::State::kDisclosure);
    } else {
      update_data.Set(
          "initialState",
          ManagedUserProfileNoticeHandler::State::kValueProposition);
    }
    update_data.Set("enforcedByPolicy",
                    create_param->profile_creation_required_by_policy);
  } else if (type == ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC) {
    update_data.Set("initialState",
                    ManagedUserProfileNoticeHandler::State::kDisclosure);
    update_data.Set("isModalDialog", true);
    update_data.Set(
        "enterpriseProfileWelcomeTitle",
        l10n_util::GetStringUTF16(IDS_ENTERPRISE_WELCOME_PROFILE_SETUP_TITLE));
    update_data.Set("profileDisclosureTitle",
                    l10n_util::GetStringUTF16(
                        IDS_ENTERPRISE_WELCOME_PROFILE_OIDC_DISCLOSURE_TITLE));

    update_data.Set("showLinkDataCheckbox", false);
  }
  if (create_param->account_info.IsManaged() == signin::Tribool::kTrue) {
    update_data.Set(
        "profileDisclosureSubtitle",
        l10n_util::GetStringFUTF16(
            IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_KNOWN_DOMAIN_SUBTITLE,
            base::UTF8ToUTF16(domain)));
  }

  if (create_param->account_info.IsManaged() != signin::Tribool::kTrue) {
    update_data.Set(
        "valuePropSubtitle",
        l10n_util::GetStringUTF16(
            base::FeatureList::IsEnabled(
                syncer::kReplaceSyncPromosWithSignInPromos)
                ? IDS_ENTERPRISE_VALUE_PROPOSITION_CONSUMER_SUBTITLE_WITH_BOOKMARKS
                : IDS_ENTERPRISE_VALUE_PROPOSITION_CONSUMER_SUBTITLE));
    update_data.Set(
        "separateBrowsingDataTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_CONSUMER_TITLE));
  } else if (create_param->user_already_signed_in ||
             base::FeatureList::IsEnabled(
                 switches::kEnforceManagementDisclaimer)) {
    update_data.Set(
        "separateBrowsingDataTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_ALREADY_SIGNED_IN_TITLE));
    update_data.Set(
        "profileDisclosureTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_ALREADY_SIGNED_IN_TITLE));
    update_data.Set(
        "profileDisclosureSubtitle",
        l10n_util::GetStringFUTF16(
            create_param->profile_creation_required_by_policy
                ? IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_ALREADY_SIGNED_IN_ENFORCED_SUBTITLE
                : IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_ALREADY_SIGNED_IN_SUBTITLE,
            base::UTF8ToUTF16(domain)));
    update_data.Set(
        "mergeBrowsingDataChoiceTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_MERGE_BROWSING_DATA_ALREADY_SIGNED_IN_CHOICE));
    update_data.Set(
        "separateBrowsingDataChoiceTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_ALREADY_SIGNED_IN_CHOICE));
    update_data.Set(
        "separateBrowsingDataChoiceDetails",
        l10n_util::GetStringFUTF16(
            IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_DATA_CHOICE_ALREADY_SIGNED_IN_DETAILS,
            base::UTF8ToUTF16(domain)));
    if (type ==
        ManagedUserProfileNoticeUI::ScreenType::kEnterpriseAccountCreation) {
      update_data.Set("cancelLabel",
                      l10n_util::GetStringUTF16(
                          create_param->profile_creation_required_by_policy
                              ? IDS_SYNC_ERROR_USER_MENU_SIGNOUT_BUTTON
                              : IDS_CANCEL));
    }
  } else if (is_school_account) {
    update_data.Set("separateBrowsingDataTitle",
                    l10n_util::GetStringUTF16(
                        IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_SCHOOL_TITLE));
    update_data.Set(
        "profileDisclosureTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_PROFILE_DISCLOSURE_SCHOOL_TITLE));
    update_data.Set("valuePropSubtitle",
                    l10n_util::GetStringUTF16(
                        IDS_ENTERPRISE_VALUE_PROPOSITION_SCHOOL_SUBTITLE));
    update_data.Set(
        "mergeBrowsingDataChoiceTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_MERGE_BROWSING_DATA_SCHOOL_CHOICE));
    update_data.Set(
        "separateBrowsingDataChoiceTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_DATA_SCHOOL_CHOICE));
  }

  // Change the text so that the "(Recommended)" label is not shown when the
  // admin has set merging data as the default option.
  bool profile_separation_data_migration_settings_optout =
      profile->GetPrefs()->GetInteger(
          prefs::kProfileSeparationDataMigrationSettings) == 2;
  bool check_link_data_checkbox_by_default_from_legacy_policy =
      g_browser_process->local_state()->GetBoolean(
          prefs::kEnterpriseProfileCreationKeepBrowsingData);
  if (create_param->show_link_data_option &&
      (profile_separation_data_migration_settings_optout ||
       check_link_data_checkbox_by_default_from_legacy_policy)) {
    update_data.Set(
        "separateBrowsingDataChoiceTitle",
        l10n_util::GetStringUTF16(
            is_school_account
                ? IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_DATA_CHOICE_SCHOOL_NOT_RECOMMENDED
                : IDS_ENTERPRISE_WELCOME_SEPARATE_BROWSING_DATA_CHOICE_WORK_NOT_RECOMMENDED));
  }

  if (create_param->show_link_data_option) {
    ProfileStatisticsFactory::GetForProfile(profile)->GatherStatistics(
        base::BindRepeating(
            &ManagedUserProfileNoticeUI::UpdateBrowsingDataStringWithCounts,
            weak_ptr_factory_.GetWeakPtr(), base::UTF8ToUTF16(domain)));
  }

  content::WebUIDataSource::Update(
      profile, chrome::kChromeUIManagedUserProfileNoticeHost,
      std::move(update_data));

  auto handler = std::make_unique<ManagedUserProfileNoticeHandler>(
      browser, type, std::move(create_param));
  handler_ = handler.get();

  web_ui()->AddMessageHandler(std::move(handler));
}

ManagedUserProfileNoticeHandler*
ManagedUserProfileNoticeUI::GetHandlerForTesting() {
  return handler_;
}

void ManagedUserProfileNoticeUI::UpdateBrowsingDataStringWithCounts(
    std::u16string domain,
    profiles::ProfileCategoryStats stats) {
  int browsing_history_count = 0;
  int bookmarks_count = 0;
  int extensions_count = 0;

  for (const auto& stat : stats) {
    if (stat.category == profiles::kProfileStatisticsBrowsingHistory) {
      browsing_history_count = stat.count;
    } else if (stat.category == profiles::kProfileStatisticsBookmarks) {
      bookmarks_count = stat.count;
    }
  }
  auto* profile = Profile::FromWebUI(web_ui());
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  extensions_count = registry->enabled_extensions().size() +
                     registry->disabled_extensions().size() +
                     registry->terminated_extensions().size() +
                     registry->blocklisted_extensions().size() +
                     registry->blocked_extensions().size();

  std::vector<std::u16string> string_replacements;
  if (bookmarks_count > 0) {
    string_replacements.push_back(
        l10n_util::GetPluralStringFUTF16(IDS_BOOKMARKS_COUNT, bookmarks_count));
  }
  if (extensions_count > 0) {
    string_replacements.push_back(l10n_util::GetPluralStringFUTF16(
        IDS_EXTENSIONS_COUNT, extensions_count));
  }
  if (browsing_history_count > 0) {
    string_replacements.push_back(l10n_util::GetPluralStringFUTF16(
        IDS_BROWSING_HISTORY_COUNT, browsing_history_count));
  }

  if (string_replacements.empty()) {
    return;
  }
  string_replacements.push_back(std::move(domain));

  base::Value::Dict update_data;
  std::u16string browsing_data_string;
  if (string_replacements.size() == 2) {
    update_data.Set(
        "mergeBrowsingDataChoiceDetails",
        l10n_util::GetStringFUTF16(
            IDS_ENTERPRISE_WELCOME_MERGE_BROWSING_DATA_WITH_ONE_COUNT_CHOICE_DETAILS,
            string_replacements, nullptr));
  }
  if (string_replacements.size() == 3) {
    update_data.Set(
        "mergeBrowsingDataChoiceDetails",
        l10n_util::GetStringFUTF16(
            IDS_ENTERPRISE_WELCOME_MERGE_BROWSING_DATA_WITH_TWO_COUNTS_CHOICE_DETAILS,
            string_replacements, nullptr));
  }
  if (string_replacements.size() == 4) {
    update_data.Set(
        "mergeBrowsingDataChoiceDetails",
        l10n_util::GetStringFUTF16(
            IDS_ENTERPRISE_WELCOME_MERGE_BROWSING_DATA_WITH_THREE_COUNTS_CHOICE_DETAILS,
            string_replacements, nullptr));
  }

  content::WebUIDataSource::Update(
      profile, chrome::kChromeUIManagedUserProfileNoticeHost,
      std::move(update_data));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ManagedUserProfileNoticeUI)
