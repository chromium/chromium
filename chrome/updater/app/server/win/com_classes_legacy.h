// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_
#define CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_

#include <windows.h>
#include <wrl/implements.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/win/app_command_runner.h"
#include "chrome/updater/win/win_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

// Definitions for COM updater classes provided for backward compatibility
// with Google Update.

namespace updater {

// TODO(crbug.com/1065712): these classes do not have to be visible in the
// updater namespace. Additionally, there is some code duplication for the
// registration and unregistration code in both server and service_main
// compilation units.
//
// This class implements the legacy Omaha3 interfaces as expected by Chrome's
// on-demand client.
class LegacyOnDemandImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IGoogleUpdate3Web,
          IAppBundleWeb,
          IAppWeb,
          ICurrentState,
          IDispatch> {
 public:
  LegacyOnDemandImpl();
  LegacyOnDemandImpl(const LegacyOnDemandImpl&) = delete;
  LegacyOnDemandImpl& operator=(const LegacyOnDemandImpl&) = delete;

  // Overrides for IGoogleUpdate3Web.
  IFACEMETHODIMP createAppBundleWeb(IDispatch** app_bundle_web) override;

  // Overrides for IAppBundleWeb.
  IFACEMETHODIMP createApp(BSTR app_id,
                           BSTR brand_code,
                           BSTR language,
                           BSTR ap) override;
  IFACEMETHODIMP createInstalledApp(BSTR app_id) override;
  IFACEMETHODIMP createAllInstalledApps() override;
  IFACEMETHODIMP get_displayLanguage(BSTR* language) override;
  IFACEMETHODIMP put_displayLanguage(BSTR language) override;
  IFACEMETHODIMP put_parentHWND(ULONG_PTR hwnd) override;
  IFACEMETHODIMP get_length(int* number) override;
  IFACEMETHODIMP get_appWeb(int index, IDispatch** app_web) override;
  IFACEMETHODIMP initialize() override;
  IFACEMETHODIMP checkForUpdate() override;
  IFACEMETHODIMP download() override;
  IFACEMETHODIMP install() override;
  IFACEMETHODIMP pause() override;
  IFACEMETHODIMP resume() override;
  IFACEMETHODIMP cancel() override;
  IFACEMETHODIMP downloadPackage(BSTR app_id, BSTR package_name) override;
  IFACEMETHODIMP get_currentState(VARIANT* current_state) override;

  // Overrides for IAppWeb.
  IFACEMETHODIMP get_appId(BSTR* app_id) override;
  IFACEMETHODIMP get_currentVersionWeb(IDispatch** current) override;
  IFACEMETHODIMP get_nextVersionWeb(IDispatch** next) override;
  IFACEMETHODIMP get_command(BSTR command_id, IDispatch** command) override;
  IFACEMETHODIMP get_currentState(IDispatch** current_state) override;
  IFACEMETHODIMP launch() override;
  IFACEMETHODIMP uninstall() override;
  IFACEMETHODIMP get_serverInstallDataIndex(BSTR* language) override;
  IFACEMETHODIMP put_serverInstallDataIndex(BSTR language) override;

  // Overrides for ICurrentState.
  IFACEMETHODIMP get_stateValue(LONG* state_value) override;
  IFACEMETHODIMP get_availableVersion(BSTR* available_version) override;
  IFACEMETHODIMP get_bytesDownloaded(ULONG* bytes_downloaded) override;
  IFACEMETHODIMP get_totalBytesToDownload(
      ULONG* total_bytes_to_download) override;
  IFACEMETHODIMP get_downloadTimeRemainingMs(
      LONG* download_time_remaining_ms) override;
  IFACEMETHODIMP get_nextRetryTime(ULONGLONG* next_retry_time) override;
  IFACEMETHODIMP get_installProgress(
      LONG* install_progress_percentage) override;
  IFACEMETHODIMP get_installTimeRemainingMs(
      LONG* install_time_remaining_ms) override;
  IFACEMETHODIMP get_isCanceled(VARIANT_BOOL* is_canceled) override;
  IFACEMETHODIMP get_errorCode(LONG* error_code) override;
  IFACEMETHODIMP get_extraCode1(LONG* extra_code1) override;
  IFACEMETHODIMP get_completionMessage(BSTR* completion_message) override;
  IFACEMETHODIMP get_installerResultCode(LONG* installer_result_code) override;
  IFACEMETHODIMP get_installerResultExtraCode1(
      LONG* installer_result_extra_code1) override;
  IFACEMETHODIMP get_postInstallLaunchCommandLine(
      BSTR* post_install_launch_command_line) override;
  IFACEMETHODIMP get_postInstallUrl(BSTR* post_install_url) override;
  IFACEMETHODIMP get_postInstallAction(LONG* post_install_action) override;

  // Overrides for IDispatch.
  IFACEMETHODIMP GetTypeInfoCount(UINT*) override;
  IFACEMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override;
  IFACEMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override;
  IFACEMETHODIMP Invoke(DISPID,
                        REFIID,
                        LCID,
                        WORD,
                        DISPPARAMS*,
                        VARIANT*,
                        EXCEPINFO*,
                        UINT*) override;

 private:
  ~LegacyOnDemandImpl() override;

  void UpdateStateCallback(UpdateService::UpdateState state_update);
  void UpdateResultCallback(UpdateService::Result result);

  // Handles the update service callbacks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Synchronized accessors.
  std::string app_id() const {
    base::AutoLock lock{lock_};
    return app_id_;
  }
  void set_app_id(const std::string& app_id) {
    base::AutoLock lock{lock_};
    app_id_ = app_id;
  }

  // Access to these members must be serialized by using the lock.
  mutable base::Lock lock_;
  std::string app_id_;
  absl::optional<UpdateService::UpdateState> state_update_;
  absl::optional<UpdateService::Result> result_;
};

// This class implements the legacy Omaha3 IProcessLauncher interface as
// expected by Chrome's setup client.
class LegacyProcessLauncherImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IProcessLauncher,
          IProcessLauncher2> {
 public:
  LegacyProcessLauncherImpl();
  LegacyProcessLauncherImpl(const LegacyProcessLauncherImpl&) = delete;
  LegacyProcessLauncherImpl& operator=(const LegacyProcessLauncherImpl&) =
      delete;

  // Overrides for IProcessLauncher/IProcessLauncher2.
  IFACEMETHODIMP LaunchCmdLine(const WCHAR* cmd_line) override;
  IFACEMETHODIMP LaunchBrowser(DWORD browser_type, const WCHAR* url) override;
  IFACEMETHODIMP LaunchCmdElevated(const WCHAR* app_id,
                                   const WCHAR* command_id,
                                   DWORD caller_proc_id,
                                   ULONG_PTR* proc_handle) override;
  IFACEMETHODIMP LaunchCmdLineEx(const WCHAR* cmd_line,
                                 DWORD* server_proc_id,
                                 ULONG_PTR* proc_handle,
                                 ULONG_PTR* stdout_handle) override;

 private:
  ~LegacyProcessLauncherImpl() override;
};

// This class implements the legacy Omaha3 IAppCommandWeb interface. AppCommands
// are a mechanism to run pre-registered command lines in the format
// `c:\path-to-exe\exe.exe param1 param2...param9` elevated. The params are
// optional and can also include replaceable parameters substituted at runtime.
//
// App commands are registered in the registry with the following formats:
// * New command layout format:
//     Update\Clients\`app_id`\Commands\`command_id`
//         REG_SZ "CommandLine" == {command format}
// * Older command layout format:
//     Update\Clients\`app_id`
//         REG_SZ `command_id` == {command format}
//
// Example {command format}: "c:\path-to\echo.exe %1 %2 %3 StaticParam4"
//
// As shown above, {command format} needs to be the complete path to an
// executable followed by optional parameters.
//
// For system applications, the registered executable path above must be in a
// secure location such as %ProgramFiles% for security, since it will be run
// elevated.
//
// Parameters can be placeholders (%1-%9) that can be filled by the numbered
// parameters in `IAppCommandWeb::execute`. Literal `%` characters must be
// escaped by doubling them.
//
// If parameters to `IAppCommandWeb::execute` are AA and BB
// respectively, a command format of:
//     `echo.exe %1 %%2 %%%2`
// becomes the command line
//     `echo.exe AA %2 %BB`
//
// Placeholders are not permitted in the process name.
//
// Placeholders may be embedded within words, and appropriate quoting of
// back-slash, double-quotes, space, and tab is applied if necessary.
class LegacyAppCommandWebImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IAppCommandWeb,
          IDispatch> {
 public:
  LegacyAppCommandWebImpl();
  LegacyAppCommandWebImpl(const LegacyAppCommandWebImpl&) = delete;
  LegacyAppCommandWebImpl& operator=(const LegacyAppCommandWebImpl&) = delete;

  // Initializes an instance of `IAppCommandWeb` for the given `scope`,
  // `app_id`, and `command_id`. Returns an error if the command format does not
  // exist in the registry, or if the command format in the registry has an
  // invalid formatting, or if the type information could not be initialized.
  HRESULT RuntimeClassInitialize(UpdaterScope scope,
                                 const std::wstring& app_id,
                                 const std::wstring& command_id);

  // Overrides for IAppCommandWeb.
  IFACEMETHODIMP get_status(UINT* status) override;
  IFACEMETHODIMP get_exitCode(DWORD* exit_code) override;
  IFACEMETHODIMP get_output(BSTR* output) override;

  // Executes the AppCommand with the optional substitutions provided. `execute`
  // fails if the number of non-empty VARIANT substitutions provided to
  // `execute` are less than the number of parameter placeholders in the
  // loaded-from-the-registry command format. Each placeholder %N is replaced
  // with the corresponding `substitutionN`.
  // An empty (VT_EMPTY) or invalid (non BSTR) substitution causes the following
  // substitutions to be ignored; for example, if `substitution2` is VT_EMPTY,
  // then `substitution3` through `substitution9` will be ignored.
  IFACEMETHODIMP execute(VARIANT substitution1,
                         VARIANT substitution2,
                         VARIANT substitution3,
                         VARIANT substitution4,
                         VARIANT substitution5,
                         VARIANT substitution6,
                         VARIANT substitution7,
                         VARIANT substitution8,
                         VARIANT substitution9) override;

  // Overrides for IDispatch.
  IFACEMETHODIMP GetTypeInfoCount(UINT* type_info_count) override;
  IFACEMETHODIMP GetTypeInfo(UINT type_info_index,
                             LCID locale_id,
                             ITypeInfo** type_info) override;
  IFACEMETHODIMP GetIDsOfNames(REFIID iid,
                               LPOLESTR* names_to_be_mapped,
                               UINT count_of_names_to_be_mapped,
                               LCID locale_id,
                               DISPID* dispatch_ids) override;
  IFACEMETHODIMP Invoke(DISPID dispatch_id,
                        REFIID iid,
                        LCID locale_id,
                        WORD flags,
                        DISPPARAMS* dispatch_parameters,
                        VARIANT* result,
                        EXCEPINFO* exception_info,
                        UINT* arg_error_index) override;

 private:
  ~LegacyAppCommandWebImpl() override;

  HRESULT InitializeTypeInfo();

  base::Process process_;
  AppCommandRunner app_command_runner_;
  Microsoft::WRL::ComPtr<ITypeInfo> type_info_;

  friend class LegacyAppCommandWebImplTest;
};

// This class implements the legacy Omaha3 IPolicyStatus interface, which
// returns the current updater policies for external constants, group policy,
// and device management.
//
// This class is used by chrome://policy to show the current updater policies.
class PolicyStatusImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IPolicyStatus,
          IPolicyStatus2,
          IPolicyStatus3,
          IDispatch> {
 public:
  PolicyStatusImpl();
  PolicyStatusImpl(const PolicyStatusImpl&) = delete;
  PolicyStatusImpl& operator=(const PolicyStatusImpl&) = delete;

  HRESULT RuntimeClassInitialize();

  // IPolicyStatus/IPolicyStatus2/IPolicyStatus3. See
  // `updater_legacy_idl.template` for the description of the properties below.
  IFACEMETHODIMP get_lastCheckPeriodMinutes(DWORD* minutes) override;
  IFACEMETHODIMP get_updatesSuppressedTimes(
      DWORD* start_hour,
      DWORD* start_min,
      DWORD* duration_min,
      VARIANT_BOOL* are_updates_suppressed) override;
  IFACEMETHODIMP get_downloadPreferenceGroupPolicy(BSTR* pref) override;
  IFACEMETHODIMP get_packageCacheSizeLimitMBytes(DWORD* limit) override;
  IFACEMETHODIMP get_packageCacheExpirationTimeDays(DWORD* days) override;
  IFACEMETHODIMP get_effectivePolicyForAppInstalls(BSTR app_id,
                                                   DWORD* policy) override;
  IFACEMETHODIMP get_effectivePolicyForAppUpdates(BSTR app_id,
                                                  DWORD* policy) override;
  IFACEMETHODIMP get_targetVersionPrefix(BSTR app_id, BSTR* prefix) override;
  IFACEMETHODIMP get_isRollbackToTargetVersionAllowed(
      BSTR app_id,
      VARIANT_BOOL* rollback_allowed) override;
  IFACEMETHODIMP get_updaterVersion(BSTR* version) override;
  IFACEMETHODIMP get_lastCheckedTime(DATE* last_checked) override;
  IFACEMETHODIMP refreshPolicies() override;
  IFACEMETHODIMP get_lastCheckPeriodMinutes(
      IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_updatesSuppressedTimes(
      IPolicyStatusValue** value,
      VARIANT_BOOL* are_updates_suppressed) override;
  IFACEMETHODIMP get_downloadPreferenceGroupPolicy(
      IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_packageCacheSizeLimitMBytes(
      IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_packageCacheExpirationTimeDays(
      IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_proxyMode(IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_proxyPacUrl(IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_proxyServer(IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_effectivePolicyForAppInstalls(
      BSTR app_id,
      IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_effectivePolicyForAppUpdates(
      BSTR app_id,
      IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_targetVersionPrefix(BSTR app_id,
                                         IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_isRollbackToTargetVersionAllowed(
      BSTR app_id,
      IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_targetChannel(BSTR app_id,
                                   IPolicyStatusValue** value) override;
  IFACEMETHODIMP get_forceInstallApps(VARIANT_BOOL is_machine,
                                      IPolicyStatusValue** value) override;

  // Overrides for IDispatch.
  IFACEMETHODIMP GetTypeInfoCount(UINT* type_info_count) override;
  IFACEMETHODIMP GetTypeInfo(UINT type_info_index,
                             LCID locale_id,
                             ITypeInfo** type_info) override;
  IFACEMETHODIMP GetIDsOfNames(REFIID iid,
                               LPOLESTR* names_to_be_mapped,
                               UINT count_of_names_to_be_mapped,
                               LCID locale_id,
                               DISPID* dispatch_ids) override;
  IFACEMETHODIMP Invoke(DISPID dispatch_id,
                        REFIID iid,
                        LCID locale_id,
                        WORD flags,
                        DISPPARAMS* dispatch_parameters,
                        VARIANT* result,
                        EXCEPINFO* exception_info,
                        UINT* arg_error_index) override;

 private:
  ~PolicyStatusImpl() override;

  scoped_refptr<PolicyService> policy_service_;
};

// This class implements the legacy Omaha3 IPolicyStatusValue interface. Each
// instance stores a single updater policy returned by the properties in
// IPolicyStatus2 and IPolicyStatus3.
class PolicyStatusValueImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IPolicyStatusValue,
          IDispatch> {
 public:
  PolicyStatusValueImpl();
  PolicyStatusValueImpl(const PolicyStatusValueImpl&) = delete;
  PolicyStatusValueImpl& operator=(const PolicyStatusValueImpl&) = delete;

  template <typename T>
  static HRESULT Create(const T& value,
                        IPolicyStatusValue** policy_status_value);

  HRESULT RuntimeClassInitialize(const std::string& source,
                                 const std::string& value,
                                 bool has_conflict,
                                 const std::string& conflict_source,
                                 const std::string& conflict_value);

  // IPolicyStatusValue. See `updater_legacy_idl.template` for the
  // description of the properties below.
  IFACEMETHODIMP get_source(BSTR* source) override;
  IFACEMETHODIMP get_value(BSTR* value) override;
  IFACEMETHODIMP get_hasConflict(VARIANT_BOOL* has_conflict) override;
  IFACEMETHODIMP get_conflictSource(BSTR* conflict_source) override;
  IFACEMETHODIMP get_conflictValue(BSTR* conflict_value) override;

  // Overrides for IDispatch.
  IFACEMETHODIMP GetTypeInfoCount(UINT* type_info_count) override;
  IFACEMETHODIMP GetTypeInfo(UINT type_info_index,
                             LCID locale_id,
                             ITypeInfo** type_info) override;
  IFACEMETHODIMP GetIDsOfNames(REFIID iid,
                               LPOLESTR* names_to_be_mapped,
                               UINT count_of_names_to_be_mapped,
                               LCID locale_id,
                               DISPID* dispatch_ids) override;
  IFACEMETHODIMP Invoke(DISPID dispatch_id,
                        REFIID iid,
                        LCID locale_id,
                        WORD flags,
                        DISPPARAMS* dispatch_parameters,
                        VARIANT* result,
                        EXCEPINFO* exception_info,
                        UINT* arg_error_index) override;

 private:
  ~PolicyStatusValueImpl() override;

  std::wstring source_;
  std::wstring value_;
  VARIANT_BOOL has_conflict_;
  std::wstring conflict_source_;
  std::wstring conflict_value_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_
