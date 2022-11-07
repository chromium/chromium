// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
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
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/win/registry.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#endif

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
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kChromeCleanerEvent[] =
    "safety-check-chrome-cleaner-status-changed";
#endif
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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
SafetyCheckHandler::ChromeCleanerStatus ConvertToChromeCleanerStatus(
    safe_browsing::ChromeCleanerController::State state,
    safe_browsing::ChromeCleanerController::IdleReason idle_reason,
    bool is_allowed_by_policy,
    bool is_cct_timestamp_known) {
  if (!is_allowed_by_policy) {
    return SafetyCheckHandler::ChromeCleanerStatus::kDisabledByAdmin;
  }
  switch (state) {
    case safe_browsing::ChromeCleanerController::State::kIdle:
      switch (idle_reason) {
        case safe_browsing::ChromeCleanerController::IdleReason::kInitial:
        case safe_browsing::ChromeCleanerController::IdleReason::
            kReporterFoundNothing:
        case safe_browsing::ChromeCleanerController::IdleReason::
            kScanningFoundNothing:
        case safe_browsing::ChromeCleanerController::IdleReason::
            kCleaningSucceeded:
          return is_cct_timestamp_known
                     ? SafetyCheckHandler::ChromeCleanerStatus::
                           kNoUwsFoundWithTimestamp
                     : SafetyCheckHandler::ChromeCleanerStatus::
                           kNoUwsFoundWithoutTimestamp;
        case safe_browsing::ChromeCleanerController::IdleReason::
            kReporterFailed:
        case safe_browsing::ChromeCleanerController::IdleReason::
            kScanningFailed:
        case safe_browsing::ChromeCleanerController::IdleReason::
            kCleaningFailed:
        case safe_browsing::ChromeCleanerController::IdleReason::
            kCleanerDownloadFailed:
          return SafetyCheckHandler::ChromeCleanerStatus::kError;
        case safe_browsing::ChromeCleanerController::IdleReason::
            kConnectionLost:
        case safe_browsing::ChromeCleanerController::IdleReason::
            kUserDeclinedCleanup:
          return SafetyCheckHandler::ChromeCleanerStatus::kInfected;
      }
    case safe_browsing::ChromeCleanerController::State::kReporterRunning:
    case safe_browsing::ChromeCleanerController::State::kScanning:
      return SafetyCheckHandler::ChromeCleanerStatus::kScanningForUws;
    case safe_browsing::ChromeCleanerController::State::kCleaning:
      return SafetyCheckHandler::ChromeCleanerStatus::kRemovingUws;
    case safe_browsing::ChromeCleanerController::State::kInfected:
      return SafetyCheckHandler::ChromeCleanerStatus::kInfected;
    case safe_browsing::ChromeCleanerController::State::kRebootRequired:
      return SafetyCheckHandler::ChromeCleanerStatus::kRebootRequired;
  }
}

SafetyCheckHandler::ChromeCleanerResult fetchChromeCleanerStatus(
    std::unique_ptr<TimestampDelegate>& timestamp_delegate) {
  SafetyCheckHandler::ChromeCleanerResult result;
  result.cct_completion_time =
      timestamp_delegate->FetchChromeCleanerScanCompletionTimestamp();
  result.status = ConvertToChromeCleanerStatus(
      safe_browsing::ChromeCleanerController::GetInstance()->state(),
      safe_browsing::ChromeCleanerController::GetInstance()->idle_reason(),
      safe_browsing::ChromeCleanerController::GetInstance()
          ->IsAllowedByPolicy(),
      !result.cct_completion_time.is_null());
  return result;
}
#endif

bool IsUnmutedCompromisedCredential(
    const extensions::api::passwords_private::PasswordUiEntry& entry) {
  DCHECK(entry.compromised_info);
  if (entry.compromised_info->is_muted)
    return false;
  return base::ranges::any_of(
      entry.compromised_info->compromise_types, [](auto type) {
        return type ==
                   extensions::api::passwords_private::COMPROMISE_TYPE_LEAKED ||
               type ==
                   extensions::api::passwords_private::COMPROMISE_TYPE_PHISHED;
      });
}

bool IsCredentialWeak(
    const extensions::api::passwords_private::PasswordUiEntry& entry) {
  DCHECK(entry.compromised_info);
  return base::ranges::any_of(
      entry.compromised_info->compromise_types, [](auto type) {
        return type == extensions::api::passwords_private::COMPROMISE_TYPE_WEAK;
      });
}

}  // namespace

base::Time TimestampDelegate::GetSystemTime() {
  return base::Time::Now();
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
base::Time TimestampDelegate::FetchChromeCleanerScanCompletionTimestamp() {
  // TODO(crbug.com/1139806): The cleaner scan completion timestamp is not
  // always written to the registry. As a workaround, it is also written to a
  // pref. This ensures that the timestamp is preserved in case Chrome is still
  // opened when the scan completes. Remove this workaround once the timestamp
  // is written to the registry in all cases.
  const base::Time end_time_from_prefs =
      g_browser_process->local_state()->GetTime(
          prefs::kChromeCleanerScanCompletionTime);

  // Read the scan completion timestamp from the registry, if it exists there.
  base::win::RegKey reporter_key;
  int64_t end_time = 0;
  if (reporter_key.Open(HKEY_CURRENT_USER,
                        chrome_cleaner::kSoftwareRemovalToolRegistryKey,
                        KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
      reporter_key.ReadInt64(chrome_cleaner::kEndTimeValueName, &end_time) !=
          ERROR_SUCCESS) {
    // TODO(crbug.com/1139806): Part of the above workaround. If the registry
    // does not contain the timestamp but the pref does, then return the one
    // from the pref.
    if (!end_time_from_prefs.is_null()) {
      return end_time_from_prefs;
    }
    // Reading failed. Return 'null' time.
    return base::Time();
  }

  // TODO(crbug.com/1139806): Part of the above workaround. If the timestamp in
  // prefs is null or older than the one from the registry, then return the one
  // from the registry. Otherwise return the one from prefs.
  base::Time end_time_from_registry =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(end_time));
  if (end_time_from_prefs.is_null() ||
      end_time_from_prefs < end_time_from_registry) {
    return end_time_from_registry;
  } else {
    return end_time_from_prefs;
  }
}
#endif

SafetyCheckHandler::SafetyCheckHandler() = default;

SafetyCheckHandler::~SafetyCheckHandler() {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // It seems |OnJavascriptDisallowed| is not always called before the
  // deconstructor. Remove the CCT observer (no-op if not registered)
  // also here to ensure it does not stay registered.
  safe_browsing::ChromeCleanerController::GetInstance()->RemoveObserver(this);
#endif
}

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
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // If the Chrome cleaner status results in the child being hidden,
  // then also hide it already in the "running" state.
  if (fetchChromeCleanerStatus(timestamp_delegate_).status ==
      SafetyCheckHandler::ChromeCleanerStatus::kHidden) {
    chrome_cleaner_status_ = SafetyCheckHandler::ChromeCleanerStatus::kHidden;
  } else {
    chrome_cleaner_status_ = SafetyCheckHandler::ChromeCleanerStatus::kChecking;
  }
#endif

  // Update WebUi.
  FireBasicSafetyCheckWebUiListener(kUpdatesEvent,
                                    static_cast<int>(update_status_),
                                    GetStringForUpdates(update_status_));
  FireBasicSafetyCheckWebUiListener(
      kPasswordsEvent, static_cast<int>(passwords_status_),
      GetStringForPasswords(passwords_status_, Compromised(0), Weak(0), Done(0),
                            Total(0)));
  FireBasicSafetyCheckWebUiListener(
      kSafeBrowsingEvent, static_cast<int>(safe_browsing_status_),
      GetStringForSafeBrowsing(safe_browsing_status_));
  FireBasicSafetyCheckWebUiListener(
      kExtensionsEvent, static_cast<int>(extensions_status_),
      GetStringForExtensions(extensions_status_, Blocklisted(0),
                             ReenabledUser(0), ReenabledAdmin(0)));
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Construct string without timestamp, using "null time" via |base::Time()|.
  FireBasicSafetyCheckWebUiListener(
      kChromeCleanerEvent, static_cast<int>(chrome_cleaner_status_),
      GetStringForChromeCleaner(chrome_cleaner_status_, base::Time(),
                                base::Time()));
#endif
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
    version_updater_.reset(VersionUpdater::Create(web_ui()->GetWebContents()));
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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  CheckChromeCleaner();
#endif
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

  // Send updated timestamp-based display strings to all SC children who have
  // such strings.
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // String update for Chrome Cleaner.
  base::Value::Dict event;
  event.Set(kNewState, static_cast<int>(chrome_cleaner_status_));
  event.Set(
      kDisplayString,
      GetStringForChromeCleaner(
          chrome_cleaner_status_,
          timestamp_delegate_->FetchChromeCleanerScanCompletionTimestamp(),
          timestamp_delegate_->GetSystemTime()));
  FireWebUIListener(kChromeCleanerEvent, event);
#endif

  // String update for the parent.
  ResolveJavascriptCallback(
      callback_id,
      base::Value(GetStringForParentRan(safety_check_completion_time_)));
}

void SafetyCheckHandler::CheckUpdates() {
  // Usage of base::Unretained(this) is safe, because we own `version_updater_`.
  version_updater_->CheckForUpdate(
      base::BindRepeating(&SafetyCheckHandler::OnVersionUpdaterResult,
                          base::Unretained(this)),
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
  extensions::ExtensionIdList extensions;
  extension_prefs_->GetExtensions(&extensions);
  int blocklisted = 0;
  int reenabled_by_user = 0;
  int reenabled_by_admin = 0;
  for (auto extension_id : extensions) {
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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
void SafetyCheckHandler::CheckChromeCleaner() {
  if (safe_browsing::ChromeCleanerController::GetInstance()->HasObserver(
          this)) {
    // Observer already registered. Just fetch the current CCT status.
    OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
  } else {
    // Registering the observer immediately triggers a callback with the
    // current state.
    safe_browsing::ChromeCleanerController::GetInstance()->AddObserver(this);
  }
  // Log the current status into metrics.
  if (chrome_cleaner_status_ != ChromeCleanerStatus::kHidden &&
      chrome_cleaner_status_ != ChromeCleanerStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.ChromeCleanerResult",
                                  chrome_cleaner_status_);
  }
  CompleteParentIfChildrenCompleted();
}
#endif

void SafetyCheckHandler::OnUpdateCheckResult(UpdateStatus status) {
  update_status_ = status;
  if (update_status_ != UpdateStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.UpdatesResult",
                                  update_status_);
  }
  // TODO(crbug/1072432): Since the UNKNOWN state is not present in JS in M83,
  // use FAILED_OFFLINE, which uses the same icon.
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
                                                Done done,
                                                Total total) {
  base::Value::Dict event;
  event.Set(kNewState, static_cast<int>(status));
  event.Set(kDisplayString,
            GetStringForPasswords(status, compromised, weak, done, total));
  FireWebUIListener(kPasswordsEvent, event);
  if (status != PasswordsStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.PasswordsResult",
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
  if (status != ExtensionsStatus::kChecking) {
    base::UmaHistogramEnumeration("Settings.SafetyCheck.ExtensionsResult",
                                  status);
  }
  extensions_status_ = status;
  CompleteParentIfChildrenCompleted();
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
void SafetyCheckHandler::OnChromeCleanerCheckResult(
    SafetyCheckHandler::ChromeCleanerResult result) {
  base::Value::Dict event;
  event.Set(kNewState, static_cast<int>(result.status));
  event.Set(kDisplayString,
            GetStringForChromeCleaner(result.status, result.cct_completion_time,
                                      timestamp_delegate_->GetSystemTime()));
  FireWebUIListener(kChromeCleanerEvent, event);
  chrome_cleaner_status_ = result.status;
}
#endif

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
          base::ASCIIToUTF16(chrome::kWhoIsMyAdministratorHelpURL));
    case UpdateStatus::kFailedOffline:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_FAILED_OFFLINE);
    case UpdateStatus::kFailed:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_UPDATES_FAILED,
          base::ASCIIToUTF16(chrome::kChromeFixUpdateProblems));
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
          base::ASCIIToUTF16(chrome::kWhoIsMyAdministratorHelpURL));
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
      if (weak.value() == 0) {
        // Only compromised passwords, no weak passwords.
        return l10n_util::GetPluralStringFUTF16(
            IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT_SHORT,
            compromised.value());
      } else {
        // Both compromised and weak passwords.
        return l10n_util::GetStringFUTF16(
            IDS_CONCAT_TWO_STRINGS_WITH_COMMA,
            l10n_util::GetPluralStringFUTF16(
                IDS_SETTINGS_COMPROMISED_PASSWORDS_COUNT_SHORT,
                compromised.value()),
            l10n_util::GetPluralStringFUTF16(
                IDS_SETTINGS_WEAK_PASSWORDS_COUNT_SHORT, weak.value()));
      }
    case PasswordsStatus::kWeakPasswordsExist:
      // Only weak passwords.
      return l10n_util::GetPluralStringFUTF16(
          IDS_SETTINGS_WEAK_PASSWORDS_COUNT_SHORT, weak.value());
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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
std::u16string SafetyCheckHandler::GetStringForChromeCleaner(
    ChromeCleanerStatus status,
    base::Time cct_completion_time,
    base::Time system_time) {
  switch (status) {
    case ChromeCleanerStatus::kHidden:
    case ChromeCleanerStatus::kChecking:
      return u"";
    case ChromeCleanerStatus::kInfected:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_INFECTED);
    case ChromeCleanerStatus::kRebootRequired:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_RESET_CLEANUP_TITLE_RESTART);
    case ChromeCleanerStatus::kScanningForUws:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_SCANNING);
    case ChromeCleanerStatus::kRemovingUws:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_REMOVING);
    case ChromeCleanerStatus::kDisabledByAdmin:
      return l10n_util::GetStringFUTF16(
          IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_DISABLED_BY_ADMIN,
          base::ASCIIToUTF16(chrome::kWhoIsMyAdministratorHelpURL));
    case ChromeCleanerStatus::kError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_ERROR);
    case ChromeCleanerStatus::kNoUwsFoundWithTimestamp:
      return SafetyCheckHandler::GetStringForChromeCleanerRan(
          cct_completion_time, system_time);
    case ChromeCleanerStatus::kNoUwsFoundWithoutTimestamp:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_NO_UWS_WITHOUT_TIMESTAMP);
  }
}
#endif

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
    // TODO(crbug.com/1015841): While a minor issue, this is not be the ideal
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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
std::u16string SafetyCheckHandler::GetStringForChromeCleanerRan(
    base::Time cct_completion_time,
    base::Time system_time) {
  if (cct_completion_time.is_null()) {
    return l10n_util::GetStringUTF16(
        IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_NO_UWS_WITHOUT_TIMESTAMP);
  }
  return SafetyCheckHandler::GetStringForTimePassed(
      cct_completion_time, system_time,
      IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_NO_UWS_WITH_TIMESTAMP_AFTER_SECONDS,
      IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_NO_UWS_WITH_TIMESTAMP_AFTER_MINUTES,
      IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_NO_UWS_WITH_TIMESTAMP_AFTER_HOURS,
      IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_NO_UWS_WITH_TIMESTAMP_YESTERDAY,
      IDS_SETTINGS_SAFETY_CHECK_CHROME_CLEANER_NO_UWS_WITH_TIMESTAMP_AFTER_DAYS);
}
#endif

void SafetyCheckHandler::DetermineIfOfflineOrError(bool connected) {
  OnUpdateCheckResult(connected ? UpdateStatus::kFailed
                                : UpdateStatus::kFailedOffline);
}

void SafetyCheckHandler::DetermineIfNoPasswordsOrSafe(
    const std::vector<extensions::api::passwords_private::PasswordUiEntry>&
        passwords) {
  OnPasswordsCheckResult(passwords.empty() ? PasswordsStatus::kNoPasswords
                                           : PasswordsStatus::kSafe,
                         Compromised(0), Weak(0), Done(0), Total(0));
}

void SafetyCheckHandler::UpdatePasswordsResultOnCheckIdle() {
  auto insecure_credentials = passwords_delegate_->GetInsecureCredentials();
  size_t num_compromised = base::ranges::count_if(
      insecure_credentials, &IsUnmutedCompromisedCredential);
  size_t num_weak =
      base::ranges::count_if(insecure_credentials, &IsCredentialWeak);

  if (num_compromised == 0 && num_weak == 0) {
    // If there are no |OnCredentialDone| callbacks with is_leaked = true, no
    // need to wait for InsecureCredentialsManager callbacks any longer, since
    // there should be none for the current password check.
    if (!compromised_passwords_exist_) {
      observed_insecure_credentials_manager_.Reset();
    }
    passwords_delegate_->GetSavedPasswordsList(
        base::BindOnce(&SafetyCheckHandler::DetermineIfNoPasswordsOrSafe,
                       base::Unretained(this)));
  } else if (num_compromised > 0) {
    // At least one compromised password. Treat as compromises.
    OnPasswordsCheckResult(PasswordsStatus::kCompromisedExist,
                           Compromised(num_compromised), Weak(num_weak),
                           Done(0), Total(0));
  } else {
    // No compromised but weak passwords. Treat as weak passwords only.
    OnPasswordsCheckResult(PasswordsStatus::kWeakPasswordsExist,
                           Compromised(num_compromised), Weak(num_weak),
                           Done(0), Total(0));
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
                             Weak(0), Done(0), Total(0));
      // Non-terminal state, so nothing else needs to be done.
      return;
    case BulkLeakCheckService::State::kSignedOut:
      OnPasswordsCheckResult(PasswordsStatus::kSignedOut, Compromised(0),
                             Weak(0), Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kNetworkError:
      OnPasswordsCheckResult(PasswordsStatus::kOffline, Compromised(0), Weak(0),
                             Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kQuotaLimit:
      OnPasswordsCheckResult(PasswordsStatus::kQuotaLimit, Compromised(0),
                             Weak(0), Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kTokenRequestFailure:
      OnPasswordsCheckResult(PasswordsStatus::kFeatureUnavailable,
                             Compromised(0), Weak(0), Done(0), Total(0));
      break;
    case BulkLeakCheckService::State::kHashingFailure:
    case BulkLeakCheckService::State::kServiceError:
      OnPasswordsCheckResult(PasswordsStatus::kError, Compromised(0), Weak(0),
                             Done(0), Total(0));
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
          extensions::api::passwords_private::PASSWORD_CHECK_STATE_RUNNING &&
      status.already_processed && status.remaining_in_queue) {
    Done done = Done(*(status.already_processed));
    Total total = Total(*(status.remaining_in_queue) + done.value());
    OnPasswordsCheckResult(PasswordsStatus::kChecking, Compromised(0), Weak(0),
                           done, total);
  }
}

void SafetyCheckHandler::OnInsecureCredentialsChanged() {
  extensions::api::passwords_private::PasswordCheckStatus status =
      passwords_delegate_->GetPasswordCheckStatus();
  // Ignore the event, unless the password check is idle with no errors.
  if (status.state !=
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_IDLE) {
    return;
  }
  UpdatePasswordsResultOnCheckIdle();
  // Stop observing the manager to avoid dynamically updating the result.
  observed_insecure_credentials_manager_.Reset();
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
void SafetyCheckHandler::OnIdle(
    safe_browsing::ChromeCleanerController::IdleReason idle_reason) {
  OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
}

void SafetyCheckHandler::OnReporterRunning() {
  OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
}

void SafetyCheckHandler::OnScanning() {
  OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
}

void SafetyCheckHandler::OnInfected(
    bool is_powered_by_partner,
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
}

void SafetyCheckHandler::OnCleaning(
    bool is_powered_by_partner,
    const safe_browsing::ChromeCleanerScannerResults& scanner_results) {
  OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
}

void SafetyCheckHandler::OnRebootRequired() {
  OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
}

void SafetyCheckHandler::OnRebootFailed() {
  OnChromeCleanerCheckResult(fetchChromeCleanerStatus(timestamp_delegate_));
}
#endif

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
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Remove |this| as an observer for the Chrome cleaner.
  safe_browsing::ChromeCleanerController::GetInstance()->RemoveObserver(this);
#endif
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
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (chrome_cleaner_status_ == ChromeCleanerStatus::kChecking) {
    return;
  }
#endif

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
