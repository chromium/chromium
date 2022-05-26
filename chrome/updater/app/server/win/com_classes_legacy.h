// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
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
  IFACEMETHODIMP LaunchCmdElevated(const WCHAR* app_guid,
                                   const WCHAR* cmd_id,
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
//
// TODO(crbug/1316682): Implement AutoRunOnOSUpgrade app commands.
class LegacyAppCommandWebImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IAppCommandWeb,
          IDispatch> {
 public:
  // Creates an instance of `IAppCommandWeb` for the given `app_id` and
  // `command_id`. Returns an error if the command format does not exist in the
  // registry, or if the command format in the registry has an invalid
  // formatting.
  static HRESULT CreateAppCommandWeb(UpdaterScope scope,
                                     const std::wstring& app_id,
                                     const std::wstring& command_id,
                                     IDispatch** app_command_web);

  LegacyAppCommandWebImpl();
  LegacyAppCommandWebImpl(const LegacyAppCommandWebImpl&) = delete;
  LegacyAppCommandWebImpl& operator=(const LegacyAppCommandWebImpl&) = delete;

  // Overrides for IAppCommandWeb.
  IFACEMETHODIMP get_status(UINT* status) override;
  IFACEMETHODIMP get_exitCode(DWORD* exit_code) override;
  IFACEMETHODIMP get_output(BSTR* output) override;

  // Executes the AppCommand with the optional parameters provided. `execute`
  // will fail if the number of non-empty VARIANT parameters provided to
  // `execute` are less than the number of parameter placeholders in the
  // loaded-from-the-registry command format. Each placeholder %N is replaced
  // with the corresponding `parameterN`.
  IFACEMETHODIMP execute(VARIANT parameter1,
                         VARIANT parameter2,
                         VARIANT parameter3,
                         VARIANT parameter4,
                         VARIANT parameter5,
                         VARIANT parameter6,
                         VARIANT parameter7,
                         VARIANT parameter8,
                         VARIANT parameter9) override;

  // Overrides for IDispatch.
  // TODO(crbug/1316683): Implement the IDispatch methods for the AppCommand
  // implementation.
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
  ~LegacyAppCommandWebImpl() override;

  static HRESULT CreateLegacyAppCommandWebImpl(
      UpdaterScope scope,
      const std::wstring& app_id,
      const std::wstring& command_id,
      Microsoft::WRL::ComPtr<LegacyAppCommandWebImpl>& web_impl);

  bool InitializeExecutable(UpdaterScope scope, const base::FilePath& exe_path);
  HRESULT Initialize(UpdaterScope scope, std::wstring command_format);

  absl::optional<std::wstring> FormatCommandLine(
      const std::vector<std::wstring>& parameters) const;

  base::FilePath executable_;
  std::vector<std::wstring> parameters_;
  base::Process process_;

  friend class LegacyAppCommandWebImplTest;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_
