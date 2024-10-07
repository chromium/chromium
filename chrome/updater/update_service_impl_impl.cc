// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl_impl.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/app/app_utils.h"
#include "chrome/updater/auto_run_on_os_upgrade_task.h"
#include "chrome/updater/change_owners_task.h"
#include "chrome/updater/check_for_updates_task.h"
#include "chrome/updater/cleanup_task.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/find_unregistered_apps_task.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/remove_uninstalled_apps_task.h"
#include "chrome/updater/update_block_check.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_usage_stats_task.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

#if BUILDFLAG(IS_MAC)
#include <sys/mount.h>
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include <winhttp.h>

#include "base/win/registry.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/resources.grh"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/win_constants.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater {

// The functions below are various adaptors between |update_client| and
// |UpdateService| types.

namespace internal {
UpdateService::Result ToResult(update_client::Error error) {
  switch (error) {
    case update_client::Error::NONE:
      return UpdateService::Result::kSuccess;
    case update_client::Error::UPDATE_IN_PROGRESS:
      return UpdateService::Result::kUpdateInProgress;
    case update_client::Error::UPDATE_CANCELED:
      return UpdateService::Result::kUpdateCanceled;
    case update_client::Error::RETRY_LATER:
      return UpdateService::Result::kRetryLater;
    case update_client::Error::SERVICE_ERROR:
      return UpdateService::Result::kServiceFailed;
    case update_client::Error::UPDATE_CHECK_ERROR:
      return UpdateService::Result::kUpdateCheckFailed;
    case update_client::Error::CRX_NOT_FOUND:
      return UpdateService::Result::kAppNotFound;
    case update_client::Error::INVALID_ARGUMENT:
    case update_client::Error::BAD_CRX_DATA_CALLBACK:
      return UpdateService::Result::kInvalidArgument;
    case update_client::Error::MAX_VALUE:
      NOTREACHED_IN_MIGRATION();
      return UpdateService::Result::kInvalidArgument;
  }
}

void GetComponents(
    scoped_refptr<PolicyService> policy_service,
    crx_file::VerifierFormat verifier_format,
    scoped_refptr<PersistedData> persisted_data,
    const base::flat_map<std::string, std::string>& app_client_install_data,
    const base::flat_map<std::string, std::string>& app_install_data_index,
    const std::string& install_source,
    UpdateService::Priority priority,
    bool update_blocked,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    const std::vector<std::string>& ids,
    base::OnceCallback<
        void(const std::vector<std::optional<update_client::CrxComponent>>&)>
        callback) {
  VLOG(1) << __func__
          << ". Same version update: " << policy_same_version_update;
  const bool is_foreground = priority == UpdateService::Priority::kForeground;
  auto barrier_callback =
      base::BarrierCallback<std::optional<update_client::CrxComponent>>(
          ids.size(),
          base::BindOnce(
              [](const std::vector<std::string>& ids,
                 const std::vector<std::optional<update_client::CrxComponent>>&
                     unordered) {
                // Re-order the vector to match the order of `ids`.
                std::vector<std::optional<update_client::CrxComponent>> ordered;
                for (const auto& id : ids) {
                  auto it = std::ranges::find_if(
                      unordered,
                      [&id](std::optional<update_client::CrxComponent> v) {
                        return v && v->app_id == id;
                      });
                  ordered.push_back(it != unordered.end() ? *it : std::nullopt);
                }
                return ordered;
              },
              ids)
              .Then(std::move(callback)));
  for (const auto& id : ids) {
    base::MakeRefCounted<Installer>(
        id,
        [&app_client_install_data, &id] {
          auto it = app_client_install_data.find(id);
          return it != app_client_install_data.end() ? it->second : "";
        }(),
        [&app_install_data_index, &id] {
          auto it = app_install_data_index.find(id);
          return it != app_install_data_index.end() ? it->second : "";
        }(),
        install_source,
        policy_service->GetTargetChannel(id).policy_or(std::string()),
        policy_service->GetTargetVersionPrefix(id).policy_or(std::string()),
        policy_service->IsRollbackToTargetVersionAllowed(id).policy_or(false),
        [&policy_service, &id, &is_foreground, update_blocked] {
          if (update_blocked) {
            return true;
          }
          PolicyStatus<int> app_updates =
              policy_service->GetPolicyForAppUpdates(id);
          return app_updates &&
                 (app_updates.policy() == kPolicyDisabled ||
                  (!is_foreground &&
                   app_updates.policy() == kPolicyManualUpdatesOnly) ||
                  (is_foreground &&
                   app_updates.policy() == kPolicyAutomaticUpdatesOnly));
        }(),
        policy_same_version_update, persisted_data, verifier_format)
        ->MakeCrxComponent(
            base::BindOnce([](update_client::CrxComponent component) {
              return component;
            }).Then(barrier_callback));
  }
}

#if BUILDFLAG(IS_WIN)
namespace {

std::wstring GetTextForUpdateClientInstallError(int error_code) {
#define INSTALL_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):     \
    return GetLocalizedStringF(IDS_GENERIC_INSTALL_ERROR_BASE, L#error_code)

  switch (error_code) {
    INSTALL_SWITCH_ENTRY(update_client::InstallError::NONE);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::FINGERPRINT_WRITE_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::BAD_MANIFEST);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::GENERIC_ERROR);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::MOVE_FILES_ERROR);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::SET_PERMISSIONS_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::INVALID_VERSION);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::VERSION_NOT_UPGRADED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::NO_DIR_COMPONENT_USER);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::CLEAN_INSTALL_DIR_FAILED);
    INSTALL_SWITCH_ENTRY(
        update_client::InstallError::INSTALL_VERIFICATION_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::MISSING_INSTALL_PARAMS);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::LAUNCH_PROCESS_FAILED);
    INSTALL_SWITCH_ENTRY(update_client::InstallError::CUSTOM_ERROR_BASE);
    default:
      return GetLocalizedStringF(IDS_GENERIC_INSTALL_ERROR_BASE,
                                 GetTextForSystemError(error_code));
  }
#undef INSTALL_SWITCH_ENTRY
}

std::wstring GetTextForDownloadError(int error) {
#define DOWNLOAD_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):      \
    return GetLocalizedStringF(IDS_GENERIC_DOWNLOAD_ERROR_BASE, L#error_code)

  switch (error) {
    DOWNLOAD_SWITCH_ENTRY(update_client::CrxDownloaderError::NO_URL);
    DOWNLOAD_SWITCH_ENTRY(update_client::CrxDownloaderError::NO_HASH);
    DOWNLOAD_SWITCH_ENTRY(
        update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS);
    DOWNLOAD_SWITCH_ENTRY(update_client::CrxDownloaderError::GENERIC_ERROR);

    case static_cast<int>(update_client::CrxDownloaderError::BAD_HASH):
      return GetLocalizedString(IDS_DOWNLOAD_HASH_MISMATCH_BASE);

    default:
      return GetLocalizedStringF(IDS_GENERIC_DOWNLOAD_ERROR_BASE,
                                 GetTextForSystemError(error));
  }
#undef DOWNLOAD_SWITCH_ENTRY
}

std::wstring GetTextForUnpackError(int error) {
#define UNPACK_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):    \
    return GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE, L#error_code)
#define UNPACK_CACHING_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):            \
    return GetLocalizedStringF(IDS_UNPACK_CACHING_ERROR_BASE, L#error_code)

  switch (error) {
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kInvalidParams);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kInvalidFile);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kUnzipPathError);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kUnzipFailed);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kBadManifest);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kBadExtension);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kIoError);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kDeltaVerificationFailure);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kDeltaBadCommands);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kDeltaUnsupportedCommand);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kDeltaOperationFailure);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kDeltaPatchProcessFailure);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kDeltaMissingExistingFile);
    UNPACK_SWITCH_ENTRY(
        update_client::UnpackerError::kPuffinMissingPreviousCrx);
    UNPACK_SWITCH_ENTRY(update_client::UnpackerError::kCrxCacheNotProvided);

    UNPACK_CACHING_SWITCH_ENTRY(
        update_client::UnpackerError::kFailedToAddToCache);
    UNPACK_CACHING_SWITCH_ENTRY(
        update_client::UnpackerError::kFailedToCreateCacheDir);

    default:
      return GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                 GetTextForSystemError(error));
  }
#undef UNPACK_SWITCH_ENTRY
#undef UNPACK_CACHING_SWITCH_ENTRY
}

std::wstring GetTextForServiceError(int error) {
#define SERVICE_SWITCH_ENTRY(error_code) \
  case static_cast<int>(error_code):     \
    return GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE, L#error_code)

  switch (error) {
    SERVICE_SWITCH_ENTRY(update_client::ServiceError::SERVICE_WAIT_FAILED);
    SERVICE_SWITCH_ENTRY(update_client::ServiceError::UPDATE_DISABLED);
    SERVICE_SWITCH_ENTRY(update_client::ServiceError::CHECK_FOR_UPDATE_ONLY);

    case static_cast<int>(update_client::ServiceError::CANCELLED):
      return GetLocalizedString(IDS_SERVICE_ERROR_CANCELLED_BASE);

    default:
      return GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE,
                                 GetTextForSystemError(error));
  }
#undef SERVICE_SWITCH_ENTRY
}

std::wstring GetTextForUpdateCheckError(int error) {
#define UPDATE_CHECK_SWITCH_ENTRY(error_code)                       \
  case static_cast<int>(error_code):                                \
    return GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE, \
                               L#error_code)

  switch (error) {
    UPDATE_CHECK_SWITCH_ENTRY(
        update_client::ProtocolError::RESPONSE_NOT_TRUSTED);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::MISSING_PUBLIC_KEY);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::MISSING_URLS);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::PARSE_FAILED);
    UPDATE_CHECK_SWITCH_ENTRY(
        update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::URL_FETCHER_FAILED);
    UPDATE_CHECK_SWITCH_ENTRY(update_client::ProtocolError::INVALID_APPID);

    case static_cast<int>(update_client::ProtocolError::UNKNOWN_APPLICATION):
      return GetLocalizedString(IDS_UNKNOWN_APPLICATION_BASE);

    case static_cast<int>(update_client::ProtocolError::RESTRICTED_APPLICATION):
      return GetLocalizedString(IDS_RESTRICTED_RESPONSE_FROM_SERVER_BASE);

    case static_cast<int>(update_client::ProtocolError::OS_NOT_SUPPORTED):
      return GetLocalizedString(IDS_OS_NOT_SUPPORTED_BASE);

    case static_cast<int>(update_client::ProtocolError::HW_NOT_SUPPORTED):
      return GetLocalizedString(IDS_HW_NOT_SUPPORTED_BASE);

    case static_cast<int>(update_client::ProtocolError::NO_HASH):
      return GetLocalizedString(IDS_NO_HASH_BASE);

    case static_cast<int>(update_client::ProtocolError::UNSUPPORTED_PROTOCOL):
      return GetLocalizedString(IDS_UNSUPPORTED_PROTOCOL_BASE);

    case static_cast<int>(update_client::ProtocolError::INTERNAL):
      return GetLocalizedString(IDS_INTERNAL_BASE);

    // Http Status Code `401` Unauthorized.
    case 401:
      return GetLocalizedString(IDS_ERROR_HTTPSTATUS_UNAUTHORIZED_BASE);

    // Http Status Code `403` Forbidden.
    case 403:
      return GetLocalizedString(IDS_ERROR_HTTPSTATUS_FORBIDDEN_BASE);

    // Http Status Code `407` Proxy Authentication Required.
    case 407:
      return GetLocalizedString(IDS_ERROR_HTTPSTATUS_PROXY_AUTH_REQUIRED_BASE);

    case HRESULT_FROM_WIN32(ERROR_WINHTTP_NAME_NOT_RESOLVED):
      return GetLocalizedStringF(IDS_NO_NETWORK_PRESENT_ERROR_BASE,
                                 GetExecutableRelativePath().value());
    default:
      return GetLocalizedStringF(
          IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
          error >= 400 && error < 600
              ? base::UTF8ToWide(base::StringPrintf("HTTP %d", error))
              : GetTextForSystemError(error));
  }
#undef UPDATE_CHECK_SWITCH_ENTRY
}

std::wstring GetTextForInstallerError(int error_code) {
#define POLICY_ERROR_SWITCH_ENTRY(error_code)                                 \
  case error_code:                                                            \
    return GetLocalizedStringF(IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE, \
                               L#error_code)

  switch (error_code) {
    POLICY_ERROR_SWITCH_ENTRY(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY);
    POLICY_ERROR_SWITCH_ENTRY(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY);
    POLICY_ERROR_SWITCH_ENTRY(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL);

    case GOOPDATEINSTALL_E_FILENAME_INVALID:
      return GetLocalizedString(IDS_INVALID_INSTALLER_FILENAME_BASE);

    case GOOPDATEINSTALL_E_INSTALLER_FAILED_START:
      return GetLocalizedString(IDS_INSTALLER_FAILED_TO_START_BASE);

    case GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT:
      return GetLocalizedString(IDS_INSTALLER_TIMED_OUT_BASE);

    case GOOPDATEINSTALL_E_INSTALL_ALREADY_RUNNING:
      return GetLocalizedStringF(
          IDS_GENERIC_INSTALLER_ERROR_BASE,
          GetTextForSystemError(ERROR_INSTALL_ALREADY_RUNNING));

    case ERROR_SUCCESS_REBOOT_INITIATED:
    case ERROR_SUCCESS_REBOOT_REQUIRED:
    case ERROR_SUCCESS_RESTART_REQUIRED:
      return GetLocalizedStringF(IDS_INSTALL_REBOOT_BASE,
                                 GetTextForSystemError(error_code));

    default:
      return GetLocalizedStringF(IDS_GENERIC_INSTALLER_ERROR_BASE,
                                 GetTextForSystemError(error_code));
  }
#undef POLICY_ERROR_SWITCH_ENTRY
}

}  // namespace

std::string GetInstallerText(UpdateService::ErrorCategory error_category,
                             int error_code,
                             int extra_code) {
  if (!error_code) {
    return {};
  }
  return base::WideToUTF8(base::StrCat(
      {[&] {
         switch (error_category) {
           case UpdateService::ErrorCategory::kInstall:
             return GetTextForUpdateClientInstallError(error_code);
           case UpdateService::ErrorCategory::kDownload:
             return GetTextForDownloadError(error_code);
           case UpdateService::ErrorCategory::kUnpack:
             return GetTextForUnpackError(error_code);
           case UpdateService::ErrorCategory::kService:
             return GetTextForServiceError(error_code);
           case UpdateService::ErrorCategory::kUpdateCheck:
             return GetTextForUpdateCheckError(error_code);
           case UpdateService::ErrorCategory::kInstaller:
             return GetTextForInstallerError(error_code);
           default:
             LOG(ERROR) << "Unknown error category: " << error_category;
             return std::wstring();
         }
       }(),
       [&] {
         if (!extra_code) {
           return std::wstring();
         }
         return base::StrCat(
             {L"\n", GetLocalizedStringF(IDS_EXTRA_CODE_BASE,
                                         base::ASCIIToWide(base::StringPrintf(
                                             "%#x", extra_code)))});
       }()}));
}
#endif  // BUILDFLAG(IS_WIN)

base::Version GetRegisteredInstallerVersion(const std::string& app_id) {
#if BUILDFLAG(IS_WIN)
  std::wstring pv;
  return base::win::RegKey(UpdaterScopeToHKeyRoot(GetUpdaterScope()),
                           GetAppClientsKey(app_id).c_str(), Wow6432(KEY_READ))
                     .ReadValue(kRegValuePV, &pv) == ERROR_SUCCESS
             ? base::Version(base::WideToUTF8(pv))
             : base::Version();
#else   // BUILDFLAG(IS_WIN)
  return {};
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace internal

namespace {

constexpr base::flat_map<std::string, std::string> kEmptyFlatMap;

update_client::Callback MakeUpdateClientCallback(
    base::OnceCallback<void(UpdateService::Result)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(UpdateService::Result)> callback,
         update_client::Error error) {
        std::move(callback).Run(internal::ToResult(error));
      },
      std::move(callback));
}

UpdateService::UpdateState::State ToUpdateState(
    update_client::ComponentState component_state) {
  switch (component_state) {
    case update_client::ComponentState::kNew:
      return UpdateService::UpdateState::State::kNotStarted;

    case update_client::ComponentState::kChecking:
      return UpdateService::UpdateState::State::kCheckingForUpdates;

    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDownloadingDiff:
      return UpdateService::UpdateState::State::kDownloading;

    case update_client::ComponentState::kCanUpdate:
      return UpdateService::UpdateState::State::kUpdateAvailable;

    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdatingDiff:
      return UpdateService::UpdateState::State::kInstalling;

    case update_client::ComponentState::kUpdated:
      return UpdateService::UpdateState::State::kUpdated;

    case update_client::ComponentState::kUpToDate:
      return UpdateService::UpdateState::State::kNoUpdate;

    case update_client::ComponentState::kUpdateError:
      return UpdateService::UpdateState::State::kUpdateError;

    case update_client::ComponentState::kRun:
    case update_client::ComponentState::kLastStatus:
      NOTREACHED_IN_MIGRATION();
      return UpdateService::UpdateState::State::kUnknown;
  }
}

UpdateService::ErrorCategory ToErrorCategory(
    update_client::ErrorCategory error_category) {
  switch (error_category) {
    case update_client::ErrorCategory::kNone:
      return UpdateService::ErrorCategory::kNone;
    case update_client::ErrorCategory::kDownload:
      return UpdateService::ErrorCategory::kDownload;
    case update_client::ErrorCategory::kUnpack:
      return UpdateService::ErrorCategory::kUnpack;
    case update_client::ErrorCategory::kInstall:
      return UpdateService::ErrorCategory::kInstall;
    case update_client::ErrorCategory::kService:
      return UpdateService::ErrorCategory::kService;
    case update_client::ErrorCategory::kUpdateCheck:
      return UpdateService::ErrorCategory::kUpdateCheck;
    case update_client::ErrorCategory::kInstaller:
      return UpdateService::ErrorCategory::kInstaller;
  }
}

update_client::UpdateClient::CrxStateChangeCallback
MakeUpdateClientCrxStateChangeCallback(
    scoped_refptr<update_client::Configurator> config,
    scoped_refptr<PersistedData> persisted_data,
    const bool new_install,
    base::RepeatingCallback<void(const UpdateService::UpdateState&)> callback) {
  return base::BindRepeating(
      [](scoped_refptr<update_client::Configurator> config,
         scoped_refptr<PersistedData> persisted_data, const bool new_install,
         base::RepeatingCallback<void(const UpdateService::UpdateState&)>
             callback,
         const update_client::CrxUpdateItem& crx_update_item) {
        UpdateService::UpdateState update_state;
        update_state.app_id = crx_update_item.id;
        update_state.state = ToUpdateState(crx_update_item.state);
        update_state.next_version = crx_update_item.next_version;
        update_state.downloaded_bytes = crx_update_item.downloaded_bytes;
        update_state.total_bytes = crx_update_item.total_bytes;
        update_state.install_progress = crx_update_item.install_progress;
        update_state.error_category =
            ToErrorCategory(crx_update_item.error_category);
        update_state.error_code = crx_update_item.error_code;
        update_state.extra_code1 = crx_update_item.extra_code1;
        if (crx_update_item.installer_result) {
          update_state.installer_cmd_line =
              crx_update_item.installer_result->installer_cmd_line;
          update_state.installer_text =
              crx_update_item.installer_result->installer_text;
#if BUILDFLAG(IS_WIN)
          if (update_state.installer_text.empty())
            update_state.installer_text = internal::GetInstallerText(
                UpdateService::ErrorCategory::kInstaller,
                update_state.error_code, update_state.extra_code1);
#endif  // BUILDFLAG(IS_WIN)
        }

        if (update_state.state == UpdateService::UpdateState::State::kUpdated ||
            update_state.state ==
                UpdateService::UpdateState::State::kUpdateError ||
            update_state.state ==
                UpdateService::UpdateState::State::kNoUpdate) {
#if BUILDFLAG(IS_WIN)
          if (update_state.installer_text.empty())
            update_state.installer_text = internal::GetInstallerText(
                update_state.error_category, update_state.error_code,
                update_state.extra_code1);
#endif  // BUILDFLAG(IS_WIN)

          // If a new install encounters an error, the AppId registered in
          // `UpdateServiceImplImpl::Install` needs to be removed here.
          // Otherwise the updater may remain installed even if there are no
          // other apps to manage, and try to update the app even though the app
          // was not installed.
          if (new_install &&
              (update_state.state ==
                   UpdateService::UpdateState::State::kUpdateError ||
               update_state.state ==
                   UpdateService::UpdateState::State::kNoUpdate)) {
            persisted_data->RemoveApp(update_state.app_id);
          }

          // Commit the prefs values written by |update_client| when the
          // update has completed, such as `pv` and `fingerprint`.
          config->GetPrefService()->CommitPendingWrite();
        }

        // TODO(crbug.com/345250525): remove dump instrumentation when fixed.
        base::debug::Alias(&update_state);
        if (update_state.app_id.empty() ||
            update_state.state == UpdateService::UpdateState::State::kUnknown) {
          base::debug::DumpWithoutCrashing();
        }
        callback.Run(update_state);
      },
      config, persisted_data, new_install, callback);
}

bool IsPathOnReadOnlyMount(const base::FilePath& path) {
  if (path.empty()) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  struct statfs fsinfo = {};
  if (statfs(path.value().c_str(), &fsinfo) == -1) {
    LOG(ERROR) << "Failed to stat: " << path;
    return false;
  }
  return fsinfo.f_flags & MNT_RDONLY;
#else
  return false;
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace

UpdateServiceImplImpl::UpdateServiceImplImpl(scoped_refptr<Configurator> config)
    : config_(config),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      update_client_(update_client::UpdateClientFactory(config)) {}

void UpdateServiceImplImpl::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::Version(kUpdaterVersion)));
}

void UpdateServiceImplImpl::FetchPolicies(
    base::OnceCallback<void(int)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetUpdaterScope() == UpdaterScope::kUser) {
    VLOG(2) << "Policy fetch skipped for user updater.";
    std::move(callback).Run(0);
  } else {
    config_->GetPolicyService()->FetchPolicies(std::move(callback));
  }
}

void UpdateServiceImplImpl::RegisterApp(
    const RegistrationRequest& request,
    base::OnceCallback<void(int)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsPathOnReadOnlyMount(request.existence_checker_path)) {
    VLOG(1) << "Existence check path " << request.existence_checker_path
            << " is on read-only file system. Registration of "
            << request.app_id << " is skipped.";
    std::move(callback).Run(kRegistrationError);
    return;
  }

  if (!IsUpdaterOrCompanionApp(request.app_id)) {
    config_->GetUpdaterPersistedData()->SetHadApps();
  }
  config_->GetUpdaterPersistedData()->RegisterApp(request);
  std::move(callback).Run(kRegistrationSuccess);
}

void UpdateServiceImplImpl::GetAppStates(
    base::OnceCallback<void(const std::vector<AppState>&)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<PersistedData> persisted_data =
      config_->GetUpdaterPersistedData();
  std::vector<std::string> app_ids = persisted_data->GetAppIds();
  std::vector<AppState> apps;
  for (const std::string& app_id : app_ids) {
    AppState app_state;
    app_state.app_id = app_id;
    app_state.version = persisted_data->GetProductVersion(app_id);
    app_state.version_path = persisted_data->GetProductVersionPath(app_id);
    app_state.version_key = persisted_data->GetProductVersionKey(app_id);
    app_state.ap = persisted_data->GetAP(app_id);
    app_state.ap_path = persisted_data->GetAPPath(app_id);
    app_state.ap_key = persisted_data->GetAPKey(app_id);
    app_state.brand_code = persisted_data->GetBrandCode(app_id);
    app_state.brand_path = persisted_data->GetBrandPath(app_id);
    app_state.ecp = persisted_data->GetExistenceCheckerPath(app_id);
    app_state.cohort = persisted_data->GetCohort(app_id);
    apps.push_back(app_state);
  }
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(apps)));
}

void UpdateServiceImplImpl::RunPeriodicTasks(base::OnceClosure callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  config_->GetUpdaterPersistedData()->SetLastStarted(
      base::Time::NowFromSystemTime());
  VLOG(1) << "last_started updated.";

  // The installer should make an updater registration, but in case it halts
  // before it does, synthesize a registration if necessary here.
  const base::Version registered_updater_version =
      config_->GetUpdaterPersistedData()->GetProductVersion(kUpdaterAppId);
  if (!registered_updater_version.IsValid() ||
      base::Version(kUpdaterVersion) > registered_updater_version) {
    RegistrationRequest updater_request;
    updater_request.app_id = kUpdaterAppId;
    updater_request.version = base::Version(kUpdaterVersion);
    RegisterApp(updater_request, base::DoNothing());
  }

  std::vector<base::OnceCallback<void(base::OnceClosure)>> new_tasks;
  new_tasks.push_back(
      base::BindOnce(&FindUnregisteredAppsTask::Run,
                     base::MakeRefCounted<FindUnregisteredAppsTask>(
                         config_, GetUpdaterScope())));
  new_tasks.push_back(
      base::BindOnce(&RemoveUninstalledAppsTask::Run,
                     base::MakeRefCounted<RemoveUninstalledAppsTask>(
                         config_, GetUpdaterScope())));
  new_tasks.push_back(base::BindOnce(
      &UpdateUsageStatsTask::Run,
      base::MakeRefCounted<UpdateUsageStatsTask>(
          GetUpdaterScope(), config_->GetUpdaterPersistedData())));
  new_tasks.push_back(MakeChangeOwnersTask(config_->GetUpdaterPersistedData(),
                                           GetUpdaterScope()));

  new_tasks.push_back(base::BindOnce(
      [](scoped_refptr<UpdateServiceImplImpl> update_service_impl,
         base::OnceClosure callback) {
        update_service_impl->FetchPolicies(base::BindOnce(
            [](base::OnceClosure callback, int /* ignore_result */) {
              std::move(callback).Run();
            },
            std::move(callback)));
      },
      base::WrapRefCounted(this)));
  new_tasks.push_back(
      base::BindOnce(&CheckForUpdatesTask::Run,
                     base::MakeRefCounted<CheckForUpdatesTask>(
                         config_, GetUpdaterScope(),
                         /*task_name=*/"UpdateAll",
                         base::BindOnce(&UpdateServiceImplImpl::UpdateAll, this,
                                        base::DoNothing()))));
  new_tasks.push_back(base::BindOnce(
      [](scoped_refptr<UpdateServiceImplImpl> self,
         base::OnceClosure callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &UpdateServiceImplImpl::ForceInstall, self, base::DoNothing(),
                base::BindOnce(
                    [](base::OnceClosure closure,
                       UpdateService::Result result) {
                      VLOG(0) << "ForceInstall task complete: " << result;
                      std::move(closure).Run();
                    },
                    std::move(callback))));
      },
      base::WrapRefCounted(this)));
  new_tasks.push_back(base::BindOnce(
      &AutoRunOnOsUpgradeTask::Run,
      base::MakeRefCounted<AutoRunOnOsUpgradeTask>(
          GetUpdaterScope(), config_->GetUpdaterPersistedData())));
  new_tasks.push_back(base::BindOnce(
      &CleanupTask::Run,
      base::MakeRefCounted<CleanupTask>(GetUpdaterScope(), config_)));

  const auto barrier_closure =
      base::BarrierClosure(new_tasks.size(), std::move(callback));
  for (auto& task : new_tasks) {
    tasks_.push(base::BindOnce(std::move(task),
                               barrier_closure.Then(base::BindRepeating(
                                   &UpdateServiceImplImpl::TaskDone, this))));
  }

  if (tasks_.size() == new_tasks.size()) {
    TaskStart();
  }
}

void UpdateServiceImplImpl::TaskStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tasks_.empty()) {
    main_task_runner_->PostTask(FROM_HERE, std::move(tasks_.front()));
  }
}

void UpdateServiceImplImpl::TaskDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tasks_.pop();
  TaskStart();
}

void UpdateServiceImplImpl::ForceInstall(
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PolicyStatus<std::vector<std::string>> force_install_apps_status =
      config_->GetPolicyService()->GetForceInstallApps();
  if (!force_install_apps_status) {
    base::BindPostTask(main_task_runner_, std::move(callback))
        .Run(UpdateService::Result::kSuccess);
    return;
  }
  std::vector<std::string> force_install_apps =
      force_install_apps_status.policy();
  CHECK(!force_install_apps.empty());

  std::vector<std::string> installed_app_ids =
      config_->GetUpdaterPersistedData()->GetAppIds();
  base::ranges::sort(force_install_apps);
  base::ranges::sort(installed_app_ids);

  std::vector<std::string> app_ids_to_install;
  base::ranges::set_difference(force_install_apps, installed_app_ids,
                               std::back_inserter(app_ids_to_install));
  if (app_ids_to_install.empty()) {
    base::BindPostTask(main_task_runner_, std::move(callback))
        .Run(UpdateService::Result::kSuccess);
    return;
  }

  VLOG(1) << __func__ << ": app_ids_to_install: "
          << base::JoinString(app_ids_to_install, " ");

  ShouldBlockUpdateForMeteredNetwork(
      Priority::kBackground,
      base::BindOnce(
          &UpdateServiceImplImpl::OnShouldBlockForceInstallForMeteredNetwork,
          this, app_ids_to_install, kEmptyFlatMap, kEmptyFlatMap,
          UpdateService::PolicySameVersionUpdate::kNotAllowed, state_update,
          std::move(callback)));
}

void UpdateServiceImplImpl::CheckForUpdate(
    const std::string& app_id,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  VLOG(1) << __func__ << ": " << app_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config_->GetUpdaterPersistedData()
           ->GetProductVersion(app_id)
           .IsValid()) {
    VLOG(1) << __func__ << ": App not registered: " << app_id;
    std::move(callback).Run(Result::kInvalidArgument);
    return;
  }

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(app_id, priority, false, policy)) {
    HandleUpdateDisabledByPolicy(app_id, policy, false, state_update,
                                 std::move(callback));
    return;
  }
  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImplImpl::OnShouldBlockCheckForUpdateForMeteredNetwork,
          this, app_id, priority, policy_same_version_update, state_update,
          std::move(callback)));
}

void UpdateServiceImplImpl::Update(
    const std::string& app_id,
    const std::string& install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config_->GetUpdaterPersistedData()
           ->GetProductVersion(app_id)
           .IsValid()) {
    std::move(callback).Run(Result::kInvalidArgument);
    return;
  }

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(app_id, priority, false, policy)) {
    HandleUpdateDisabledByPolicy(app_id, policy, false, state_update,
                                 std::move(callback));
    return;
  }
  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImplImpl::OnShouldBlockUpdateForMeteredNetwork, this,
          std::vector<std::string>{app_id}, kEmptyFlatMap,
          base::flat_map<std::string, std::string>(
              {std::make_pair(app_id, install_data_index)}),
          priority, policy_same_version_update, state_update,
          std::move(callback)));
}

void UpdateServiceImplImpl::UpdateAll(
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto app_ids = config_->GetUpdaterPersistedData()->GetAppIds();
  CHECK(base::Contains(
      app_ids, base::ToLowerASCII(kUpdaterAppId),
      static_cast<std::string (*)(std::string_view)>(&base::ToLowerASCII)));

  const Priority priority = Priority::kBackground;
  ShouldBlockUpdateForMeteredNetwork(
      priority,
      base::BindOnce(
          &UpdateServiceImplImpl::OnShouldBlockUpdateForMeteredNetwork, this,
          app_ids, kEmptyFlatMap, kEmptyFlatMap, priority,
          UpdateService::PolicySameVersionUpdate::kNotAllowed, state_update,
          base::BindOnce(
              [](base::OnceCallback<void(Result)> callback,
                 scoped_refptr<PersistedData> persisted_data, Result result) {
                if (result == Result::kSuccess) {
                  persisted_data->SetLastChecked(
                      base::Time::NowFromSystemTime());
                  VLOG(1) << "last_checked updated.";
                }
                std::move(callback).Run(result);
              },
              std::move(callback), config_->GetUpdaterPersistedData())));
}

void UpdateServiceImplImpl::Install(
    const RegistrationRequest& registration,
    const std::string& client_install_data,
    const std::string& install_data_index,
    Priority priority,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  VLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(registration.app_id, priority, true, policy)) {
    HandleUpdateDisabledByPolicy(registration.app_id, policy, true,
                                 state_update, std::move(callback));
    return;
  }
  if (!IsUpdaterOrCompanionApp(registration.app_id)) {
    config_->GetUpdaterPersistedData()->SetHadApps();
  }

  const bool new_install = !config_->GetUpdaterPersistedData()
                                ->GetProductVersion(registration.app_id)
                                .IsValid();
  if (new_install) {
    // Pre-register the app if there is no registration for it. This app
    // registration is removed later if the app install encounters an error.
    config_->GetUpdaterPersistedData()->RegisterApp(registration);
  } else {
    // Update ap.
    RegistrationRequest request;
    request.app_id = registration.app_id;
    request.ap = registration.ap;
    config_->GetUpdaterPersistedData()->RegisterApp(request);
  }

  std::multimap<std::string, base::RepeatingClosure>::iterator pos =
      cancellation_callbacks_.emplace(registration.app_id, base::DoNothing());
  pos->second = update_client_->Install(
      registration.app_id,
      base::BindOnce(
          &internal::GetComponents, config_->GetPolicyService(),
          config_->GetCrxVerifierFormat(), config_->GetUpdaterPersistedData(),
          base::flat_map<std::string, std::string>(
              {std::make_pair(registration.app_id, client_install_data)}),
          base::flat_map<std::string, std::string>(
              {std::make_pair(registration.app_id, install_data_index)}),
          kInstallSourceTaggedMetainstaller, priority,
          /*update_blocked=*/false, PolicySameVersionUpdate::kAllowed),
      MakeUpdateClientCrxStateChangeCallback(config_,
                                             config_->GetUpdaterPersistedData(),
                                             new_install, state_update),
      MakeUpdateClientCallback(std::move(callback))
          .Then(base::BindOnce(
              [](scoped_refptr<UpdateServiceImplImpl> self,
                 const std::multimap<std::string,
                                     base::RepeatingClosure>::iterator& pos) {
                self->cancellation_callbacks_.erase(pos);
              },
              base::WrapRefCounted(this), pos)));
}

void UpdateServiceImplImpl::CancelInstalls(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  auto [first, last] = cancellation_callbacks_.equal_range(app_id);
  base::ranges::for_each(first, last, [](const auto& i) { i.second.Run(); });
}

void UpdateServiceImplImpl::RunInstaller(
    const std::string& app_id,
    const base::FilePath& installer_path,
    const std::string& install_args,
    const std::string& install_data,
    const std::string& install_settings,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  VLOG(1) << __func__ << ": " << app_id << ": " << installer_path << ": "
          << install_args << ": " << install_data << ": " << install_settings;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int policy = kPolicyEnabled;
  if (IsUpdateDisabledByPolicy(app_id, Priority::kForeground, true, policy)) {
    HandleUpdateDisabledByPolicy(app_id, policy, true, state_update,
                                 std::move(callback));
    return;
  }

  const base::Version pv =
      config_->GetUpdaterPersistedData()->GetProductVersion(app_id);
  AppInfo app_info(
      GetUpdaterScope(), app_id,
      pv.IsValid() ? config_->GetUpdaterPersistedData()->GetAP(app_id) : "",
      pv.IsValid() ? config_->GetUpdaterPersistedData()->GetBrandCode(app_id)
                   : "",
      pv,
      pv.IsValid()
          ? config_->GetUpdaterPersistedData()->GetExistenceCheckerPath(app_id)
          : base::FilePath());

  const base::Version installer_version([&install_settings]() -> std::string {
    std::unique_ptr<base::Value> install_settings_deserialized =
        JSONStringValueDeserializer(install_settings)
            .Deserialize(
                /*error_code=*/nullptr, /*error_message=*/nullptr);
    if (install_settings_deserialized) {
      const base::Value::Dict* install_settings_dict =
          install_settings_deserialized->GetIfDict();
      if (install_settings_dict) {
        const std::string* installer_version_value =
            install_settings_dict->FindString(kInstallerVersion);
        if (installer_version_value) {
          return *installer_version_value;
        }
      }
    }

    return {};
  }());

  // Create a task runner that:
  //   1) has SequencedTaskRunner::CurrentDefaultHandle set, to run
  //      `state_update` callback.
  //   2) may block, since `RunApplicationInstaller` blocks.
  //   3) has `base::WithBaseSyncPrimitives()`, since `RunApplicationInstaller`
  //      waits on process.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const AppInfo& app_info, const base::FilePath& installer_path,
             const std::string& install_args, const std::string& install_data,
             base::RepeatingCallback<void(const UpdateState&)> state_update,
             bool usage_stats_enabled) {
            base::ScopedTempDir temp_dir;
            if (!temp_dir.CreateUniqueTempDir()) {
              return InstallerResult(
                  {.category_ = update_client::ErrorCategory::kInstall,
                   .code_ = kErrorCreatingTempDir,
#if BUILDFLAG(IS_WIN)
                   .extra_ = HRESULTFromLastError()
#else
                   .extra_ = logging::GetLastSystemErrorCode()
#endif  // BUILDFLAG(IS_WIN)
                  });
            }

            return RunApplicationInstaller(
                app_info, installer_path, install_args,
                WriteInstallerDataToTempFile(temp_dir.GetPath(), install_data),
                usage_stats_enabled, kWaitForAppInstaller,
                base::BindRepeating(
                    [](base::RepeatingCallback<void(const UpdateState&)>
                           state_update,
                       const std::string& app_id, int progress) {
                      VLOG(4) << "Install progress: " << progress;
                      UpdateState state;
                      state.app_id = app_id;
                      state.state = UpdateState::State::kInstalling;
                      state.install_progress = progress;
                      state_update.Run(state);
                    },
                    state_update, app_info.app_id));
          },
          app_info, installer_path, install_args, install_data, state_update,
          config_->GetUpdaterPersistedData()->GetUsageStatsEnabled() ||
              AreRawUsageStatsEnabled(GetUpdaterScope())),
      base::BindOnce(
          [](scoped_refptr<Configurator> config,
             scoped_refptr<PersistedData> persisted_data,
             scoped_refptr<update_client::UpdateClient> update_client,
             base::Version installer_version,
             base::RepeatingCallback<void(const UpdateState&)> state_update,
             const std::string& app_id, const std::string& ap,
             const std::string& brand,
             base::OnceCallback<void(Result)> callback,
             const InstallerResult& result) {
            // Final state update after installation completes.
            UpdateState state;
            state.app_id = app_id;
            state.state =
                result.result.category_ == update_client::ErrorCategory::kNone
                    ? UpdateState::State::kUpdated
                    : UpdateState::State::kUpdateError;

            const base::Version registered_version =
                internal::GetRegisteredInstallerVersion(app_id);
            if (registered_version.IsValid()) {
              VLOG(1) << app_id << " registered_version " << registered_version
                      << " overrides the original installer_version "
                      << installer_version;
              installer_version = registered_version;
            }

            if (result.result.category_ ==
                    update_client::ErrorCategory::kNone &&
                installer_version.IsValid()) {
              persisted_data->SetProductVersion(app_id, installer_version);
              config->GetPrefService()->CommitPendingWrite();
            }

            state.error_category = ToErrorCategory(result.result.category_);
            state.error_code = result.result.code_;
            state.extra_code1 = result.result.extra_;
            state.installer_text = result.installer_text;
#if BUILDFLAG(IS_WIN)
            if (state.installer_text.empty())
              state.installer_text = internal::GetInstallerText(
                  state.error_category, state.error_code, state.extra_code1);
#endif  // BUILDFLAG(IS_WIN)
            state.installer_cmd_line = result.installer_cmd_line;
            state_update.Run(state);
            VLOG(1) << app_id
                    << " installation completed: " << state.error_code;

            if (!persisted_data->GetEulaRequired()) {
              // Send an install ping. In some environments the ping cannot be
              // sent, so do not wait for it to be sent before calling back the
              // client.
              update_client::CrxComponent install_data;
              install_data.ap = ap;
              install_data.app_id = app_id;
              install_data.brand = brand;
              install_data.requires_network_encryption = false;
              install_data.install_source = kInstallSourceOffline;
              install_data.version = installer_version;
              update_client->SendPing(
                  install_data,
                  {.event_type = update_client::protocol_request::kEventInstall,
                   .result = result.result.category_ ==
                             update_client::ErrorCategory::kNone,
                   .error_category = result.result.category_,
                   .error_code = result.result.code_,
                   .extra_code1 = result.result.extra_},
                  base::DoNothing());
            }

            std::move(callback).Run(result.result.category_ ==
                                            update_client::ErrorCategory::kNone
                                        ? Result::kSuccess
                                        : Result::kInstallFailed);
          },
          config_, config_->GetUpdaterPersistedData(), update_client_,
          installer_version, state_update, app_info.app_id, app_info.ap,
          app_info.brand, std::move(callback)));
}

bool UpdateServiceImplImpl::IsUpdateDisabledByPolicy(const std::string& app_id,
                                                     Priority priority,
                                                     bool is_install,
                                                     int& policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  policy = kPolicyEnabled;

  if (is_install) {
    PolicyStatus<int> app_install_policy_status =
        config_->GetPolicyService()->GetPolicyForAppInstalls(app_id);
    if (app_install_policy_status) {
      policy = app_install_policy_status.policy();
    }
    return app_install_policy_status &&
           (policy == kPolicyDisabled || (config_->IsPerUserInstall() &&
                                          policy == kPolicyEnabledMachineOnly));
  } else {
    PolicyStatus<int> app_update_policy_status =
        config_->GetPolicyService()->GetPolicyForAppUpdates(app_id);
    if (app_update_policy_status) {
      policy = app_update_policy_status.policy();
    }
    return app_update_policy_status &&
           (policy == kPolicyDisabled ||
            ((policy == kPolicyManualUpdatesOnly) &&
             (priority != Priority::kForeground)) ||
            ((policy == kPolicyAutomaticUpdatesOnly) &&
             (priority == Priority::kForeground)));
  }
}

void UpdateServiceImplImpl::HandleUpdateDisabledByPolicy(
    const std::string& app_id,
    int policy,
    bool is_install,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateState update_state;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kUpdateError;
  update_state.error_category = UpdateService::ErrorCategory::kInstaller;
  update_state.error_code =
      is_install ? GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY
      : policy != kPolicyAutomaticUpdatesOnly
          ? GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY
          : GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL;
  update_state.extra_code1 = 0;
#if BUILDFLAG(IS_WIN)
  update_state.installer_text = internal::GetInstallerText(
      update_state.error_category, update_state.error_code,
      update_state.extra_code1);
#endif  // BUILDFLAG(IS_WIN)

  base::BindPostTask(main_task_runner_, state_update).Run(update_state);
  base::BindPostTask(main_task_runner_, std::move(callback))
      .Run(UpdateService::Result::kUpdateCheckFailed);
}

void UpdateServiceImplImpl::OnShouldBlockCheckForUpdateForMeteredNetwork(
    const std::string& app_id,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback,
    bool update_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &update_client::UpdateClient::CheckForUpdate, update_client_, app_id,
          base::BindOnce(&internal::GetComponents, config_->GetPolicyService(),
                         config_->GetCrxVerifierFormat(),
                         config_->GetUpdaterPersistedData(), kEmptyFlatMap,
                         kEmptyFlatMap,
                         priority == UpdateService::Priority::kForeground
                             ? kInstallSourceOnDemand
                             : "",
                         priority, update_blocked, policy_same_version_update),
          MakeUpdateClientCrxStateChangeCallback(
              config_, config_->GetUpdaterPersistedData(),
              /*new_install=*/false, state_update),
          priority == Priority::kForeground,
          MakeUpdateClientCallback(std::move(callback))));
}

void UpdateServiceImplImpl::OnShouldBlockUpdateForMeteredNetwork(
    const std::vector<std::string>& app_ids,
    const base::flat_map<std::string, std::string>& app_client_install_data,
    const base::flat_map<std::string, std::string>& app_install_data_index,
    Priority priority,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback,
    bool update_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &update_client::UpdateClient::Update, update_client_, app_ids,
          base::BindOnce(&internal::GetComponents, config_->GetPolicyService(),
                         config_->GetCrxVerifierFormat(),
                         config_->GetUpdaterPersistedData(),
                         app_client_install_data, app_install_data_index,
                         priority == UpdateService::Priority::kForeground
                             ? kInstallSourceOnDemand
                             : "",
                         priority, update_blocked, policy_same_version_update),
          MakeUpdateClientCrxStateChangeCallback(
              config_, config_->GetUpdaterPersistedData(),
              /*new_install=*/false, state_update),
          priority == Priority::kForeground,
          MakeUpdateClientCallback(std::move(callback))));
}

void UpdateServiceImplImpl::OnShouldBlockForceInstallForMeteredNetwork(
    const std::vector<std::string>& app_ids,
    const base::flat_map<std::string, std::string>& app_client_install_data,
    const base::flat_map<std::string, std::string>& app_install_data_index,
    PolicySameVersionUpdate policy_same_version_update,
    base::RepeatingCallback<void(const UpdateState&)> state_update,
    base::OnceCallback<void(Result)> callback,
    bool update_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The result from Install is only used for logging. Thus, arbitrarily pick
  // the first non-success result to propagate.
  auto barrier_callback = base::BarrierCallback<Result>(
      app_ids.size(),
      base::BindOnce([](const std::vector<Result>& results) {
        auto error_it = base::ranges::find_if(
            results, [](Result result) { return result != Result::kSuccess; });
        return error_it == std::end(results) ? Result::kSuccess : *error_it;
      }).Then(std::move(callback)));

  for (const std::string& id : app_ids) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&update_client::UpdateClient::Install),
            update_client_, id,
            base::BindOnce(&internal::GetComponents,
                           config_->GetPolicyService(),
                           config_->GetCrxVerifierFormat(),
                           config_->GetUpdaterPersistedData(),
                           app_client_install_data, app_install_data_index,
                           kInstallSourcePolicy, Priority::kBackground,
                           update_blocked, policy_same_version_update),
            MakeUpdateClientCrxStateChangeCallback(
                config_, config_->GetUpdaterPersistedData(),
                /*new_install=*/false, state_update),
            MakeUpdateClientCallback(barrier_callback)));
  }
}

UpdateServiceImplImpl::~UpdateServiceImplImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_->GetPrefService()->SchedulePendingLossyWrites();
}

}  // namespace updater
