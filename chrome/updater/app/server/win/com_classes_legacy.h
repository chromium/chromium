// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_
#define CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_

#include <windows.h>

#include <wrl/implements.h>

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/types/expected.h"
#include "base/win/win_util.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/app_command_runner.h"
#include "chrome/updater/win/setup/setup_util.h"

// Definitions for COM updater classes provided for backward compatibility
// with Google Update.

namespace updater {

namespace {

template <typename TDualInterface, typename... TInterfaces>
using WrlRuntimeDispatchClass = Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IDispatch,
    TDualInterface,
    TInterfaces...>;

}  // namespace

// The `IDispatchImpl` class implements `IDispatch` for interface
// `TDualInterface`, where `TDualInterface` is a dual interface. The IDispatch
// implementation relies on the typelib/typeinfo for interface `TDualInterface`.
//
// If the class supports more interfaces other than `TDualInterface`, these
// interfaces can be passed in via `TInterfaces...`.
//
// The `user_iid_map` and `system_iid_map` passed to the constructor are to
// allow for distinct TypeLibs to be registered and marshaled for user/system.
// See the code below for examples.
//
// Note that the `IDispatchImpl` class only implements the `IDispatch` methods
// for the `TDualInterface` interface.
template <typename TDualInterface, typename... TInterfaces>
class IDispatchImpl
    : public WrlRuntimeDispatchClass<TDualInterface, TInterfaces...> {
 public:
  IDispatchImpl(const base::flat_map<IID, IID, IidComparator>& user_iid_map,
                const base::flat_map<IID, IID, IidComparator>& system_iid_map)
      : iid_map_(IsSystemInstall() ? system_iid_map : user_iid_map),
        hr_load_typelib_(InitializeTypeInfo()) {}
  IDispatchImpl(const IDispatchImpl&) = default;
  IDispatchImpl& operator=(const IDispatchImpl&) = default;
  ~IDispatchImpl() override = default;

  // IUnknown override.
  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    const auto find_iid = iid_map_.find(riid);
    return WrlRuntimeDispatchClass<TDualInterface, TInterfaces...>::
        QueryInterface(find_iid != iid_map_.end() ? find_iid->second : riid,
                       object);
  }

  // Overrides for IDispatch.
  IFACEMETHODIMP GetTypeInfoCount(UINT* type_info_count) override {
    if (FAILED(hr_load_typelib_)) {
      return hr_load_typelib_;
    }

    *type_info_count = 1;
    return S_OK;
  }

  IFACEMETHODIMP GetTypeInfo(UINT type_info_index,
                             LCID locale_id,
                             ITypeInfo** type_info) override {
    if (FAILED(hr_load_typelib_)) {
      return hr_load_typelib_;
    }

    return type_info_index == 0 ? type_info_.CopyTo(type_info) : E_INVALIDARG;
  }

  IFACEMETHODIMP GetIDsOfNames(REFIID iid,
                               LPOLESTR* names_to_be_mapped,
                               UINT count_of_names_to_be_mapped,
                               LCID locale_id,
                               DISPID* dispatch_ids) override {
    if (FAILED(hr_load_typelib_)) {
      return hr_load_typelib_;
    }

    return type_info_->GetIDsOfNames(names_to_be_mapped,
                                     count_of_names_to_be_mapped, dispatch_ids);
  }

  IFACEMETHODIMP Invoke(DISPID dispatch_id,
                        REFIID iid,
                        LCID locale_id,
                        WORD flags,
                        DISPPARAMS* dispatch_parameters,
                        VARIANT* result,
                        EXCEPINFO* exception_info,
                        UINT* arg_error_index) override {
    if (FAILED(hr_load_typelib_)) {
      return hr_load_typelib_;
    }

    HRESULT hr = type_info_->Invoke(
        Microsoft::WRL::ComPtr<TDualInterface>(this).Get(), dispatch_id, flags,
        dispatch_parameters, result, exception_info, arg_error_index);

    LOG_IF(ERROR, FAILED(hr)) << __func__ << " type_info_->Invoke failed, "
                              << dispatch_id << ", " << std::hex << hr;
    return hr;
  }

  // Loads the typelib and typeinfo for interface `TDualInterface`.
  HRESULT InitializeTypeInfo() {
    base::FilePath typelib_path;
    if (!base::PathService::Get(base::DIR_EXE, &typelib_path)) {
      return E_UNEXPECTED;
    }

    typelib_path =
        typelib_path.Append(GetExecutableRelativePath())
            .Append(GetComTypeLibResourceIndex(__uuidof(TDualInterface)));

    Microsoft::WRL::ComPtr<ITypeLib> type_lib;
    if (HRESULT hr = ::LoadTypeLib(typelib_path.value().c_str(), &type_lib);
        FAILED(hr)) {
      LOG(ERROR) << __func__ << " ::LoadTypeLib failed, " << typelib_path
                 << ", " << std::hex << hr
                 << ", IID: " << StringFromGuid(__uuidof(TDualInterface));
      return hr;
    }

    if (HRESULT hr =
            type_lib->GetTypeInfoOfGuid(__uuidof(TDualInterface), &type_info_);
        FAILED(hr)) {
      LOG(ERROR) << __func__ << " ::GetTypeInfoOfGuid failed" << ", "
                 << std::hex << hr
                 << ", IID: " << StringFromGuid(__uuidof(TDualInterface));
      return hr;
    }

    return S_OK;
  }

 private:
  const base::flat_map<IID, IID, IidComparator> iid_map_;
  Microsoft::WRL::ComPtr<ITypeInfo> type_info_;
  const HRESULT hr_load_typelib_;
};

// This class implements the legacy Omaha3 IGoogleUpdate3Web interface as
// expected by Chrome's on-demand client.
class LegacyOnDemandImpl : public IDispatchImpl<IGoogleUpdate3Web> {
 public:
  LegacyOnDemandImpl();
  LegacyOnDemandImpl(const LegacyOnDemandImpl&) = delete;
  LegacyOnDemandImpl& operator=(const LegacyOnDemandImpl&) = delete;

  // Overrides for IGoogleUpdate3Web.
  IFACEMETHODIMP createAppBundleWeb(IDispatch** app_bundle_web) override;

 private:
  ~LegacyOnDemandImpl() override;
};

// This class implements the legacy Omaha3 IProcessLauncher interface as
// expected by Chrome's setup client.
class LegacyProcessLauncherImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IProcessLauncher,
          IProcessLauncherSystem,
          IProcessLauncher2,
          IProcessLauncher2System> {
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
class LegacyAppCommandWebImpl : public IDispatchImpl<IAppCommandWeb> {
 public:
  struct ErrorParams {
    int error_code = 0;
    int extra_code1 = 0;
  };

  using PingSender = base::RepeatingCallback<void(UpdaterScope scope,
                                                  const std::string& app_id,
                                                  const std::string& command_id,
                                                  ErrorParams error_params)>;
  LegacyAppCommandWebImpl();
  LegacyAppCommandWebImpl(const LegacyAppCommandWebImpl&) = delete;
  LegacyAppCommandWebImpl& operator=(const LegacyAppCommandWebImpl&) = delete;

  // Initializes an instance of `IAppCommandWeb` for the given `scope`,
  // `app_id`, `command_id`, and a `ping_sender`. Returns an error if the
  // command format does not exist in the registry, or if the command format in
  // the registry has an invalid formatting, or if the type information could
  // not be initialized.
  HRESULT RuntimeClassInitialize(UpdaterScope scope,
                                 const std::wstring& app_id,
                                 const std::wstring& command_id,
                                 PingSender ping_sender = base::BindRepeating(
                                     &LegacyAppCommandWebImpl::SendPing));

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

  const base::Process& process() const { return process_; }

 private:
  friend class LegacyAppCommandWebImplTest;

  static void SendPing(UpdaterScope scope,
                       const std::string& app_id,
                       const std::string& command_id,
                       ErrorParams error_params);

  ~LegacyAppCommandWebImpl() override;

  base::Process process_;
  HResultOr<AppCommandRunner> app_command_runner_;
  UpdaterScope scope_ = UpdaterScope::kSystem;
  std::string app_id_;
  std::string command_id_;
  PingSender ping_sender_ = base::DoNothing();
};

// This class implements the legacy Omaha3 IPolicyStatus* interfaces, which
// return the current updater policies for external constants, group policy,
// and device management.
//
// This class is used by chrome://policy to show the current updater policies.
class PolicyStatusImpl : public IDispatchImpl<IPolicyStatus4,
                                              IPolicyStatus3,
                                              IPolicyStatus2,
                                              IPolicyStatus> {
 public:
  PolicyStatusImpl();
  PolicyStatusImpl(const PolicyStatusImpl&) = delete;
  PolicyStatusImpl& operator=(const PolicyStatusImpl&) = delete;

  HRESULT RuntimeClassInitialize();

  // IPolicyStatus/IPolicyStatus2/IPolicyStatus3/IPolicyStatus4. See
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
  IFACEMETHODIMP get_cloudPolicyOverridesPlatformPolicy(
      IPolicyStatusValue** value) override;

 private:
  ~PolicyStatusImpl() override;

  scoped_refptr<PolicyService> policy_service_;
};

// This class implements the legacy Omaha3 IPolicyStatusValue interface. Each
// instance stores a single updater policy returned by the properties in
// IPolicyStatus2 and IPolicyStatus3.
class PolicyStatusValueImpl : public IDispatchImpl<IPolicyStatusValue> {
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
