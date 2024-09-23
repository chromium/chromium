// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace {

// Constants for communication with JS.
constexpr char kParentEvent[] = "safety-check-parent-status-changed";
constexpr char kUpdatesEvent[] = "safety-check-updates-status-changed";
constexpr char kPasswordsEvent[] = "safety-check-passwords-status-changed";
constexpr char kSafeBrowsingEvent[] =
    "safety-check-safe-browsing-status-changed";
constexpr char kExtensionsEvent[] = "safety-check-extensions-status-changed";
constexpr char kPerformSafetyCheck[] = "performSafetyCheck";
constexpr char kGetParentRanDisplayString[] = "getSafetyCheckRanDisplayString";
constexpr char kNewState[] = "newState";
constexpr char kDisplayString[] = "displayString";

// Converts the VersionUpdater::Status to the UpdateStatus enum to be passed
// to the safety check frontend. Note: if the VersionUpdater::Status gets
// changed, this will fail to compile. That is done intentionally to ensure
// that the states of the safety check are always in sync with the
// VersionUpdater ones.
SafetyCheckHandler::UpdateStatus ConvertToUpdateStatus(
    VersionUpdater::Status status) {
  switch (status) {
    case VersionUpdater::CHECKING:
      return SafetyCheckHandler::UpdateStatus::kChecking;
    case VersionUpdater::UPDATED:
      return SafetyCheckHandler::UpdateStatus::kUpdated;
    case VersionUpdater::UPDATING:
      return SafetyCheckHandler::UpdateStatus::kUpdating;
    case VersionUpdater::DEFERRED:
    case VersionUpdater::NEED_PERMISSION_TO_UPDATE:
    case VersionUpdater::NEARLY_UPDATED:
      return SafetyCheckHandler::UpdateStatus::kRelaunch;
    case VersionUpdater::DISABLED_BY_ADMIN:
      return SafetyCheckHandler::UpdateStatus::kDisabledByAdmin;
    case VersionUpdater::UPDATE_TO_ROLLBACK_VERSION_DISALLOWED:
      return SafetyCheckHandler::UpdateStatus::
          kUpdateToRollbackVersionDisallowed;
    // The disabled state can only be returned on non Chrome-branded browsers.
    case VersionUpdater::DISABLED:
      return SafetyCheckHandler::UpdateStatus::kUnknown;
    case VersionUpdater::FAILED:
    case VersionUpdater::FAILED_HTTP:
    case VersionUpdater::FAILED_DOWNLOAD:
    case VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED:
      return SafetyCheckHandler::UpdateStatus::kFailed;
    case VersionUpdater::FAILED_OFFLINE:
      return SafetyCheckHandler::UpdateStatus::kFailedOffline;
  }
}

bool IsUnmutedCompromisedCredential(
    const extensions::api::passwords_private::PasswordUiEntry& entry) {
  DCHECK(entry.compromised_info);
  if (entry.compromised_info->is_muted)
    return false;
  return base::ranges::any_of(
      entry.compromised_info->compromise_types, [](auto type) {
        return type == extensions::api::passwords_private::CompromiseType::
                           kLeaked ||
               type ==
                   extensions::api::passwords_private::CompromiseType::kPhished;
      });
}

bool IsCredentialWeak(
    const extensions::api::passwords_private::PasswordUiEntry& entry) {
  DCHECK(entry.compromised_info);
  return base::ranges::any_of(
      entry.compromised_info->compromise_types, [](auto type) {
        return type ==
               extensions::api::passwords_private::CompromiseType::kWeak;
      });
}

bool IsCredentialReused(
    const extensions::api::passwords_private::PasswordUiEntry& entry) {
  DCHECK(entry.compromised_info);
  return base::ranges::any_of(
      entry.compromised_info->compromise_types, [](auto type) {
        return type ==
               extensions::api::passwords_private::CompromiseType::kReused;
      });
}

}  // namespace

base::Time TimestampDelegate::GetSystemTime() {
  return base::Time::Now();
}

SafetyCheckHandler::SafetyCheckHandler() = default;

SafetyCheckHandler::~SafetyCheckHandler() = default;

void SafetyCheckHandler::SendSafetyCheckStartedWebUiUpdates() {
  AllowJavascript();

  // Ensure necessary delegates and helpers exist.
  if (!timestamp_delegate_) {
    timestamp_delegate_ = std::make_unique<TimestampDelegate>();
  }
  DCHECK(timestamp_delegate_);

  // Reset status of parent and children, which might have been set from a
  // previous run of safety check.
  parent_status_ = ParentStatus::kChecking;
  update_status_ = UpdateStatus::kChecking;
  passwords_status_ = PasswordsStatus::kChecking;
  safe_browsing_status_ = SafeBrowsingStatus::kChecking;
  extensions_status_ = ExtensionsStatus::kChecking;

  // Update WebUi.
  FireBasicSafetyCheckWebUiListener(kUpdatesEvent,
                                    static_cast<int>(update_status_),
                                    GetStringForUpdates(update_status_));
  FireBasicSafetyCheckWebUiListener(
      kPasswordsEvent, static_cast<int>(passwords_status_),
      GetStringForPasswords(passwords_status_, Compromised(0), Weak(0),
                            Reused(0), Done(0), Total(0)));
  FireBasicSafetyCheckWebUiListener(
      kSafeBrowsingEvent, static_cast<int>(safe_browsing_status_),
      GetStringForSafeBrowsing(safe_browsing_status_));
  FireBasicSafetyCheckWebUiListener(
      kExtensionsEvent, static_cast<int>(extensions_status_),
      GetStringForExtensions(extensions_status_, Blocklisted(0),
                             ReenabledUser(0), ReenabledAdmin(0)));
  // Parent update is last as it reveals the children elements.
  FireBasicSafetyCheckWebUiListener(kParentEvent,
                                    static_cast<int>(parent_status_),
                                    GetStringForParent(parent_status_));
}

void SafetyCheckHandler::PerformSafetyCheck() {
  // Checks common to desktop, Android, and iOS are handled by
  // safety_check::SafetyCheck.
  safe_browsing_status_ =
      safety_check::CheckSafeBrowsing(Profile::FromWebUI(web_ui())->GetPrefs());
  if (safe_browsing_status_ != SafeBrowsingStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.SafeBrowsingResult",
                                  safe_browsing_status_);
  }
  FireBasicSafetyCheckWebUiListener(
      kSafeBrowsingEvent, static_cast<int>(safe_browsing_status_),
      GetStringForSafeBrowsing(safe_browsing_status_));

  if (!version_updater_) {
    version_updater_ = VersionUpdater::Create(web_ui()->GetWebContents());
  }
  DCHECK(version_updater_);
  if (!update_helper_) {
    update_helper_ = std::make_unique<safety_check::UpdateCheckHelper>(
        Profile::FromWebUI(web_ui())
            ->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess());
  }
  DCHECK(update_helper_);
  CheckUpdates();

  if (!leak_service_) {
    leak_service_ = BulkLeakCheckServiceFactory::GetForProfile(
        Profile::FromWebUI(web_ui()));
  }
  DCHECK(leak_service_);
  if (!passwords_delegate_) {
    passwords_delegate_ =
        extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(
            Profile::FromWebUI(web_ui()), true);
  }
  DCHECK(passwords_delegate_);
  if (!insecure_credentials_manager_) {
    insecure_credentials_manager_ =
        passwords_delegate_->GetInsecureCredentialsManager();
  }
  DCHECK(insecure_credentials_manager_);
  CheckPasswords();

  if (!extension_prefs_) {
    extension_prefs_ = extensions::ExtensionPrefsFactory::GetForBrowserContext(
        Profile::FromWebUI(web_ui()));
  }
  DCHECK(extension_prefs_);
  if (!extension_service_) {
    extension_service_ =
        extensions::ExtensionSystem::Get(Profile::FromWebUI(web_ui()))
            ->extension_service();
  }
  DCHECK(extension_service_);
  CheckExtensions();
}

SafetyCheckHandler::SafetyCheckHandler(
    std::unique_ptr<safety_check::UpdateCheckHelper> update_helper,
    std::unique_ptr<VersionUpdater> version_updater,
    password_manager::BulkLeakCheckService* leak_service,
    extensions::PasswordsPrivateDelegate* passwords_delegate,
    extensions::ExtensionPrefs* extension_prefs,
    extensions::ExtensionServiceInterface* extension_service,
    std::unique_ptr<TimestampDelegate> timestamp_delegate)
    : update_helper_(std::move(update_helper)),
      version_updater_(std::move(version_updater)),
      leak_service_(leak_service),
      passwords_delegate_(passwords_delegate),
      extension_prefs_(extension_prefs),
      extension_service_(extension_service),
      timestamp_delegate_(std::move(timestamp_delegate)) {}

void SafetyCheckHandler::HandlePerformSafetyCheck(
    const base::Value::List& args) {
  SendSafetyCheckStartedWebUiUpdates();

  // Run safety check after a delay. This ensures that the "running" state is
  // visible to users for each safety check child, even if a child would
  // otherwise complete in an instant.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SafetyCheckHandler::PerformSafetyCheck,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(1));
}

void SafetyCheckHandler::HandleGetParentRanDisplayString(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];

  // String update for the parent.
  ResolveJavascriptCallback(
      callback_id,
      base::Value(GetStringForParentRan(safety_check_completion_time_)));
}

void SafetyCheckHandler::CheckUpdates() {
  version_updater_->CheckForUpdate(
      base::BindRepeating(&SafetyCheckHandler::OnVersionUpdaterResult,
                          weak_ptr_factory_.GetWeakPtr()),
      VersionUpdater::PromoteCallback());
}

void SafetyCheckHandler::CheckPasswords() {
  // Reset the tracking for callbacks with compromised passwords.
  compromised_passwords_exist_ = false;
  // Remove |this| as an existing observer for BulkLeakCheck if it is
  // registered. This takes care of an edge case when safety check starts twice
  // on the same page. Normally this should not happen, but if it does, the
  // browser should not crash.
  observed_leak_check_.Reset();
  observed_leak_check_.Observe(leak_service_.get());
  // Start observing the InsecureCredentialsManager.
  observed_insecure_credentials_manager_.Reset();
  observed_insecure_credentials_manager_.Observe(
      insecure_credentials_manager_.get());
  passwords_delegate_->StartPasswordCheck(base::BindOnce(
      &SafetyCheckHandler::OnStateChanged, weak_ptr_factory_.GetWeakPtr()));
}

void SafetyCheckHandler::CheckExtensions() {
  int blocklisted = 0;
  int reenabled_by_user = 0;
  int reenabled_by_admin = 0;
  for (const auto& extension_id : extension_prefs_->GetExtensions()) {
    extensions::BitMapBlocklistState state =
        extensions::blocklist_prefs::GetExtensionBlocklistState(
            extension_id, extension_prefs_);
    if (state == extensions::BitMapBlocklistState::NOT_BLOCKLISTED) {
      continue;
    }
    ++blocklisted;
    if (!extension_service_->IsExtensionEnabled(extension_id)) {
      continue;
    }
    if (extension_service_->UserCanDisableInstalledExtension(extension_id)) {
      ++reenabled_by_user;
    } else {
      ++reenabled_by_admin;
    }
  }
  if (blocklisted == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kNoneBlocklisted, Blocklisted(0),
                            ReenabledUser(0), ReenabledAdmin(0));
  } else if (reenabled_by_user == 0 && reenabled_by_admin == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedAllDisabled,
                            Blocklisted(blocklisted), ReenabledUser(0),
                            ReenabledAdmin(0));
  } else if (reenabled_by_user > 0 && reenabled_by_admin == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedReenabledAllByUser,
                            Blocklisted(blocklisted),
                            ReenabledUser(reenabled_by_user),
                            ReenabledAdmin(0));
  } else if (reenabled_by_admin > 0 && reenabled_by_user == 0) {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedReenabledAllByAdmin,
                            Blocklisted(blocklisted), ReenabledUser(0),
                            ReenabledAdmin(reenabled_by_admin));
  } else {
    OnExtensionsCheckResult(ExtensionsStatus::kBlocklistedReenabledSomeByUser,
                            Blocklisted(blocklisted),
                            ReenabledUser(reenabled_by_user),
                            ReenabledAdmin(reenabled_by_admin));
  }
}

void SafetyCheckHandler::OnUpdateCheckResult(UpdateStatus status) {
  update_status_ = status;
  if (update_status_ != UpdateStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.UpdatesResult",
                                  update_status_);
  }
  // TODO(crbug.com/40127188): Since the UNKNOWN state is not present in JS in
  // M83, use FAILED_OFFLINE, which uses the same icon.
  FireBasicSafetyCheckWebUiListener(
      kUpdatesEvent,
      static_cast<int>(update_status_ != UpdateStatus::kUnknown
                           ? update_status_
                           : UpdateStatus::kFailedOffline),
      GetStringForUpdates(update_status_));
  CompleteParentIfChildrenCompleted();
}

void SafetyCheckHandler::OnPasswordsCheckResult(PasswordsStatus status,
                                                Compromised compromised,
                                                Weak weak,
                                                Reused reused,
                                                Done done,
                                                Total total) {
  base::Value::Dict event;
  event.Set(kNewState, static_cast<int>(status));
  event.Set(kDisplayString, GetStringForPasswords(status, compromised, weak,
                                                  reused, done, total));
  FireWebUIListener(kPasswordsEvent, event);
  if (status != PasswordsStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.PasswordsResult2",
                                  status);
  }
  passwords_status_ = status;
  CompleteParentIfChildrenCompleted();
}

void SafetyCheckHandler::OnExtensionsCheckResult(
    ExtensionsStatus status,
    Blocklisted blocklisted,
    ReenabledUser reenabled_user,
    ReenabledAdmin reenabled_admin) {
  base::Value::Dict event;
  event.Set(kNewState, static_cast<int>(status));
  event.Set(kDisplayString,
            GetStringForExtensions(status, Blocklisted(blocklisted),
                                   reenabled_user, reenabled_admin));
  FireWebUIListener(kExtensionsEvent, event);
  extensions_status_ = status;
  CompleteParentIfChildrenCompleted();
}

std::u16string SafetyCheckHandler::GetStringForParent(ParentStatus status) {
  switch (status) {
    case ParentStatus::kBefore:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_BEFORE);
    case ParentStatus::kChecking:
      return l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_CHECK_RUNNING);
    case ParentStatus::kAfter:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER);
  }
}

std::u16string SafetyCheckHandler::GetStringForUpdates(UpdateStatus status) {
  switch (status) {
    case UpdateStatus::kChecking:
      return u"";
    case UpdateStatus::kUpdated:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      return ui::SubstituteChromeOSDeviceType(IDS_SETTINGS_UPGRADE_UP_TO_DATE);
#else
      return l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UP_TO_DATE);
#endif
    case UpdateStatus::kUpdating:
      return l10n_util::GetStringUTF16(IDS_SETTINGS_UPGRADE_UPDATING);
    case UpdateStatus::kRelaunch:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_UPGRADE_SUCCESSFUL_RELAUNCH);
    case UpdateStatus::kDisabledByAdmin:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_DISABLED_BY_ADMIN,
          chrome::kWhoIsMyAdministratorHelpURL);
    // This status is only used in ChromeOS.
    case UpdateStatus::kUpdateToRollbackVersionDisallowed:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_UPDATE_TO_ROLLBACK_VERSION_DISALLOWED);
    case UpdateStatus::kFailedOffline:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_FAILED_OFFLINE);
    case UpdateStatus::kFailed:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_FAILED,
          chrome::kChromeFixUpdateProblems);
    case UpdateStatus::kUnknown:
      return VersionUI::GetAnnotatedVersionStringForUi();
    // This state is only used on Android for recording metrics. This codepath
    // is unreachable.
    case UpdateStatus::kOutdated:
      return u"";
  }
}

std::u16string SafetyCheckHandler::GetStringForSafeBrowsing(
    SafeBrowsingStatus status) {
  switch (status) {
    case SafeBrowsingStatus::kChecking:
      return u"";
    case SafeBrowsingStatus::kEnabled:
    case SafeBrowsingStatus::kEnabledStandard:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENABLED_STANDARD);
    case SafeBrowsingStatus::kEnabledEnhanced:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENABLED_ENHANCED);
    case SafeBrowsingStatus::kDisabled:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED);
    case SafeBrowsingStatus::kDisabledByAdmin:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED_BY_ADMIN,
          chrome::kWhoIsMyAdministratorHelpURL);
    case SafeBrowsingStatus::kDisabledByExtension:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED_BY_EXTENSION);
    case SafeBrowsingStatus::kEnabledStandardAvailableEnhanced:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENABLED_STANDARD_AVAILABLE_ENHANCED);
  }
}

std::u16string SafetyCheckHandler::GetStringForPasswords(
    PasswordsStatus status,
    Compromised compromised,
    Weak weak,
    Reused reused,
    Done done,
    Total total) {
  switch (status) {
    case PasswordsStatus::kChecking: {
      // Unable to get progress for some reason.
      if (total.value() == 0) {
        return u"";
      }
      return l10n_util::GetStringFUTF16(IDS_SETTINGS_CHECK_PASSWORDS_PROGRESS,
                                        base::FormatNumber(done.value()),
                                        base::FormatNumber(total.value()));
    }
    case PasswordsStatus::kSafe:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT, 0);
    case PasswordsStatus::kCompromisedExist:
    case PasswordsStatus::kWeakPasswordsExist:
    case PasswordsStatus::kReusedPasswordsExist:
    case PasswordsStatus::kMutedCompromisedExist: {
      // Keep the order since compromised issues should come first, then weak,
      // then reused.
      std::vector<std::u16string> issues;
      if (compromised.value()) {
        issues.push_back(l10n_util::GetPluralStringFUTF16(
            IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT_SHORT,
            compromised.value()));
      }
      if (weak.value()) {
        issues.push_back(l10n_util::GetPluralStringFUTF16(
            IDS_SETTINGS_WEAK_PASSWORDS_COUNT_SHORT, weak.value()));
      }
      if (reused.value()) {
        issues.push_back(l10n_util::GetPluralStringFUTF16(
            IDS_SETTINGS_REUSED_PASSWORDS_COUNT_SHORT, reused.value()));
      }

      CHECK(!issues.empty());
      if (issues.size() == 1) {
        return issues[0];
      }
      if (issues.size() == 2) {
        return l10n_util::GetStringFUTF16(IDS_CONCAT_TWO_STRINGS_WITH_COMMA,
                                          issues[0], issues[1]);
      }
      return l10n_util::GetStringFUTF16(IDS_CONCAT_THREE_STRINGS_WITH_COMMA,
                                        issues[0], issues[1], issues[2]);
    }
    case PasswordsStatus::kOffline:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_OFFLINE);
    case PasswordsStatus::kNoPasswords:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_NO_PASSWORDS);
    case PasswordsStatus::kSignedOut:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_SIGNED_OUT);
    case PasswordsStatus::kQuotaLimit:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_QUOTA_LIMIT);
    case PasswordsStatus::kError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CHECK_PASSWORDS_ERROR_GENERIC);
    case PasswordsStatus::kFeatureUnavailable:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_PASSWORDS_FEATURE_UNAVAILABLE);
  }
}

std::u16string SafetyCheckHandler::GetStringForExtensions(
    ExtensionsStatus status,
    Blocklisted blocklisted,
    ReenabledUser reenabled_user,
    ReenabledAdmin reenabled_admin) {
  switch (status) {
    case ExtensionsStatus::kChecking:
      return u"";
    case ExtensionsStatus::kNoneBlocklisted:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_SAFE);
    case ExtensionsStatus::kBlocklistedAllDisabled:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_OFF,
          blocklisted.value());
    case ExtensionsStatus::kBlocklistedReenabledAllByUser:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_USER,
          reenabled_user.value());
    case ExtensionsStatus::kBlocklistedReenabledSomeByUser:
      return l10n_util::GetStringFUTF16(
          IDS_CONCAT_TWO_STRINGS_WITH_PERIODS,
          l10n_util::GetPluralStringFUTF16(
              IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_USER,
              reenabled_user.value()),
          l10n_util::GetPluralStringFUTF16(
              IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_ADMIN,
              reenabled_admin.value()));
    case ExtensionsStatus::kBlocklistedReenabledAllByAdmin:
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_ON_ADMIN,
          reenabled_admin.value());
  }
}

std::u16string SafetyCheckHandler::GetStringForTimePassed(
    base::Time completion_timestamp,
    base::Time system_time,
    int less_than_one_minute_ago_message_id,
    int minutes_ago_message_id,
    int hours_ago_message_id,
    int yesterday_message_id,
    int days_ago_message_id) {
  base::Time::Exploded completion_time_exploded;
  completion_timestamp.LocalExplode(&completion_time_exploded);

  base::Time::Exploded system_time_exploded;
  system_time.LocalExplode(&system_time_exploded);

  const base::Time time_yesterday = system_time - base::Days(1);
  base::Time::Exploded time_yesterday_exploded;
  time_yesterday.LocalExplode(&time_yesterday_exploded);

  const auto time_diff = system_time - completion_timestamp;
  if (completion_time_exploded.year == system_time_exploded.year &&
      completion_time_exploded.month == system_time_exploded.month &&
      completion_time_exploded.day_of_month ==
          system_time_exploded.day_of_month) {
    // The timestamp is today.
    const int time_diff_in_mins = time_diff.InMinutes();
    if (time_diff_in_mins == 0) {
      return l10n_util::GetStringUTF16(less_than_one_minute_ago_message_id);
    } else if (time_diff_in_mins < 60) {
      return l10n_util::GetPluralStringFUTF16(minutes_ago_message_id,
                                              time_diff_in_mins);
    } else {
      return l10n_util::GetPluralStringFUTF16(hours_ago_message_id,
                                              time_diff_in_mins / 60);
    }
  } else if (completion_time_exploded.year == time_yesterday_exploded.year &&
             completion_time_exploded.month == time_yesterday_exploded.month &&
             completion_time_exploded.day_of_month ==
                 time_yesterday_exploded.day_of_month) {
    // The timestamp was yesterday.
    return l10n_util::GetStringUTF16(yesterday_message_id);
  } else {
    // The timestamp is longer ago than yesterday.
    // TODO(crbug.com/40103878): While a minor issue, this is not be the ideal
    // way to calculate the days passed since the timestamp. For example,
    // <48 h might still be 2 days ago.
    const int time_diff_in_days = time_diff.InDays();
    return l10n_util::GetPluralStringFUTF16(days_ago_message_id,
                                            time_diff_in_days);
  }
}

std::u16string SafetyCheckHandler::GetStringForParentRan(
    base::Time safety_check_completion_time,
    base::Time system_time) {
  return SafetyCheckHandler::GetStringForTimePassed(
      safety_check_completion_time, system_time,
      IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER,
      IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_MINS,
      IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_HOURS,
      IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_YESTERDAY,
      IDS_SETTINGS_SAFETY_CHECK_PARENT_PRIMARY_LABEL_AFTER_DAYS);
}

std::u16string SafetyCheckHandler::GetStringForParentRan(
    base::Time safety_check_completion_time) {
  return SafetyCheckHandler::GetStringForParentRan(safety_check_completion_time,
                                                   base::Time::Now());
}

void SafetyCheckHandler::DetermineIfOfflineOrError(bool connected) {
  OnUpdateCheckResult(connected ? UpdateStatus::kFailed
                                : UpdateStatus::kFailedOffline);
}

void SafetyCheckHandler::DetermineIfNoPasswordsOrSafe(
    const std::vector<extensions::api::passwords_private::PasswordUiEntry>&
        passwords) {
  OnPasswordsCheckResult(passwords.empty() ? PasswordsStatus::kNoPasswords
                                           : PasswordsStatus::kSafe,
                         Compromised(0), Weak(0), Reused(0), Done(0), Total(0));
}

void SafetyCheckHandler::UpdatePasswordsResultOnCheckIdle() {
  auto insecure_credentials = passwords_delegate_->GetInsecureCredentials();
  size_t num_compromised = base::ranges::count_if(
      insecure_credentials, &IsUnmutedCompromisedCredential);
  size_t num_weak =
      base::ranges::count_if(insecure_credentials, &IsCredentialWeak);
  size_t num_reused =
      base::ranges::count_if(insecure_credentials, &IsCredentialReused);

  if (num_compromised > 0) {
    // At least one compromised password. Treat as compromises.
    OnPasswordsCheckResult(PasswordsStatus::kCompromisedExist,
                           Compromised(num_compromised), Weak(num_weak),
                           Reused(num_reused), Done(0), Total(0));
  } else if (num_weak > 0) {
    // No compromised but weak passwords. Treat as weak passwords only.
    OnPasswordsCheckResult(PasswordsStatus::kWeakPasswordsExist,
                           Compromised(num_compromised), Weak(num_weak),
                           Reused(num_reused), Done(0), Total(0));
  } else if (num_reused > 0) {
    // No weak or compromised but reused passwords.
    OnPasswordsCheckResult(PasswordsStatus::kReusedPasswordsExist,
                           Compromised(num_compromised), Weak(num_weak),
                           Reused(num_reused), Done(0), Total(0));
  } else {
    // If there are no |OnCredentialDone| callbacks with is_leaked = true, no
    // need to wait for InsecureCredentialsManager callbacks any longer, since
    // there should be none for the current password check.
    if (!compromised_passwords_exist_) {
      observed_insecure_credentials_manager_.Reset();
    }
    passwords_delegate_->GetSavedPasswordsList(
        base::BindOnce(&SafetyCheckHandler::DetermineIfNoPasswordsOrSafe,
                       base::Unretained(this)));
  }
}

void SafetyCheckHandler::OnVersionUpdaterResult(VersionUpdater::Status status,
                                                int progress,
                                                bool rollback,
                                                bool powerwash,
                                                const std::string& version,
                                                int64_t update_size,
                                                const std::u16string& message) {
  if (status == VersionUpdater::FAILED) {
    update_helper_->CheckConnectivity(
        base::BindOnce(&SafetyCheckHandler::DetermineIfOfflineOrError,
                       base::Unretained(this)));
    return;
  }
  OnUpdateCheckResult(ConvertToUpdateStatus(status));
}

void SafetyCheckHandler::OnStateChanged(
    password_manager::BulkLeakCheckService::State state) {
  using password_manager::BulkLeakCheckService;
  switch (state) {
    case BulkLeakCheckService::State::kIdle:
    case BulkLeakCheckService::State::kCanceled: {
      UpdatePasswordsResultOnCheckIdle();
      observed_leak_check_.Reset();
      return;
    }
    case BulkLeakCheckService::State::kRunning:
      OnPasswordsCheckResult(PasswordsStatus::kChecking, Compromised(0),
                             Weak(0), Reused(0), Done(0), Total(0));
      // Non-terminal state, so nothing else needs to be done.
      return;
    case BulkLeakCheckService::State::kSignedOut:
      OnPasswordsCheckResult(PasswordsStatus::kSignedOut, Compromised(0),
                             Weak(0), Reused(0), Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kNetworkError:
      OnPasswordsCheckResult(PasswordsStatus::kOffline, Compromised(0), Weak(0),
                             Reused(0), Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kQuotaLimit:
      OnPasswordsCheckResult(PasswordsStatus::kQuotaLimit, Compromised(0),
                             Weak(0), Reused(0), Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kTokenRequestFailure:
      OnPasswordsCheckResult(PasswordsStatus::kFeatureUnavailable,
                             Compromised(0), Weak(0), Reused(0), Done(0),
                             Total(0));
      break;
    case BulkLeakCheckService::State::kHashingFailure:
    case BulkLeakCheckService::State::kServiceError:
      OnPasswordsCheckResult(PasswordsStatus::kError, Compromised(0), Weak(0),
                             Reused(0), Done(0), Total(0));
      break;
  }

  // Stop observing the leak service and credentials manager in all non-idle
  // states.
  observed_leak_check_.Reset();
  observed_insecure_credentials_manager_.Reset();
}

void SafetyCheckHandler::OnCredentialDone(
    const password_manager::LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  // If a leaked credential is discovered, this is guaranteed to not be a safe
  // state.
  if (is_leaked) {
    compromised_passwords_exist_ = true;
  }
  extensions::api::passwords_private::PasswordCheckStatus status =
      passwords_delegate_->GetPasswordCheckStatus();
  // Send progress updates only if the check is still running.
  if (status.state ==
          extensions::api::passwords_private::PasswordCheckState::kRunning &&
      status.already_processed && status.remaining_in_queue) {
    Done done = Done(*(status.already_processed));
    Total total = Total(*(status.remaining_in_queue) + done.value());
    OnPasswordsCheckResult(PasswordsStatus::kChecking, Compromised(0), Weak(0),
                           Reused(0), done, total);
  }
}

void SafetyCheckHandler::OnInsecureCredentialsChanged() {
  extensions::api::passwords_private::PasswordCheckStatus status =
      passwords_delegate_->GetPasswordCheckStatus();
  // Ignore the event, unless the password check is idle with no errors.
  if (status.state !=
      extensions::api::passwords_private::PasswordCheckState::kIdle) {
    return;
  }
  UpdatePasswordsResultOnCheckIdle();
  // Stop observing the manager to avoid dynamically updating the result.
  observed_insecure_credentials_manager_.Reset();
}

void SafetyCheckHandler::OnJavascriptAllowed() {}

void SafetyCheckHandler::OnJavascriptDisallowed() {
  // If the user refreshes the Settings tab in the delay between starting safety
  // check and now, then the check should no longer be run. Invalidating the
  // pointer prevents the callback from returning after the delay.
  weak_ptr_factory_.InvalidateWeakPtrs();
  // Remove |this| as an observer for BulkLeakCheck. This takes care of an edge
  // case when the page is reloaded while the password check is in progress and
  // another safety check is started. Otherwise |observed_leak_check_|
  // automatically calls RemoveAll() on destruction.
  observed_leak_check_.Reset();
  // Remove |this| as an observer for InsecureCredentialsManager. This takes
  // care of an edge case where an observation would happen when Javascript is
  // already disabled. See crbug/1370719.
  observed_insecure_credentials_manager_.Reset();
  // Destroy the version updater to prevent getting a callback and firing a
  // WebUI event, which would cause a crash.
  version_updater_.reset();
}

void SafetyCheckHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      kPerformSafetyCheck,
      base::BindRepeating(&SafetyCheckHandler::HandlePerformSafetyCheck,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kGetParentRanDisplayString,
      base::BindRepeating(&SafetyCheckHandler::HandleGetParentRanDisplayString,
                          base::Unretained(this)));
}

void SafetyCheckHandler::CompleteParentIfChildrenCompleted() {
  if (update_status_ == UpdateStatus::kChecking ||
      passwords_status_ == PasswordsStatus::kChecking ||
      safe_browsing_status_ == SafeBrowsingStatus::kChecking ||
      extensions_status_ == ExtensionsStatus::kChecking) {
    return;
  }

  // All children checks completed.
  parent_status_ = ParentStatus::kAfter;
  // Remember when safety check completed.
  safety_check_completion_time_ = base::Time::Now();
  // Update UI.
  FireBasicSafetyCheckWebUiListener(kParentEvent,
                                    static_cast<int>(parent_status_),
                                    GetStringForParent(parent_status_));
}

void SafetyCheckHandler::FireBasicSafetyCheckWebUiListener(
    const std::string& event_name,
    int new_state,
    const std::u16string& display_string) {
  base::Value::Dict event;
  event.Set(kNewState, new_state);
  event.Set(kDisplayString, display_string);
  FireWebUIListener(event_name, event);
}
