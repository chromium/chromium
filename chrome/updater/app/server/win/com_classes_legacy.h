// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_
#define CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_

#include <windows.h>
#include <wrl/implements.h>

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/update_service.h"

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
  base::Optional<UpdateService::UpdateState> state_update_;
  base::Optional<UpdateService::Result> result_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_LEGACY_H_
