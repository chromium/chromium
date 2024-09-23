// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_H_
#define CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_H_

#include <windows.h>

#include <wrl/implements.h>

#include <string>

#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/util/win_util.h"

namespace updater {

// This class implements the IUpdateState interface and exposes it as a COM
// object. The purpose of this class is to remote the state of the
// |UpdateService|. Instances of this class are typically passed as arguments
// to RPC method calls which model COM events.
class UpdateStateImpl : public DYNAMICIIDSIMPL(IUpdateState) {
 public:
  explicit UpdateStateImpl(const UpdateService::UpdateState& update_state)
      : update_state_(update_state) {}
  UpdateStateImpl(const UpdateStateImpl&) = delete;
  UpdateStateImpl& operator=(const UpdateStateImpl&) = delete;

  // Overrides for IUpdateState.
  IFACEMETHODIMP get_state(LONG* state) override;
  IFACEMETHODIMP get_appId(BSTR* app_id) override;
  IFACEMETHODIMP get_nextVersion(BSTR* next_version) override;
  IFACEMETHODIMP get_downloadedBytes(LONGLONG* downloaded_bytes) override;
  IFACEMETHODIMP get_totalBytes(LONGLONG* total_bytes) override;
  IFACEMETHODIMP get_installProgress(LONG* install_progress) override;
  IFACEMETHODIMP get_errorCategory(LONG* error_category) override;
  IFACEMETHODIMP get_errorCode(LONG* error_code) override;
  IFACEMETHODIMP get_extraCode1(LONG* extra_code1) override;
  IFACEMETHODIMP get_installerText(BSTR* installer_text) override;
  IFACEMETHODIMP get_installerCommandLine(BSTR* installer_cmd_line) override;

 private:
  ~UpdateStateImpl() override = default;

  const UpdateService::UpdateState update_state_;
};

// This class implements the ICompleteStatus interface and exposes it as a COM
// object.
class CompleteStatusImpl : public DYNAMICIIDSIMPL(ICompleteStatus) {
 public:
  CompleteStatusImpl(int code, const std::wstring& message)
      : code_(code), message_(message) {}
  CompleteStatusImpl(const CompleteStatusImpl&) = delete;
  CompleteStatusImpl& operator=(const CompleteStatusImpl&) = delete;

  // Overrides for ICompleteStatus.
  IFACEMETHODIMP get_statusCode(LONG* code) override;
  IFACEMETHODIMP get_statusMessage(BSTR* message) override;

 private:
  ~CompleteStatusImpl() override = default;

  const int code_;
  const std::wstring message_;
};

// This class implements the IUpdater interface and exposes it as a COM object.
class UpdaterImpl : public DYNAMICIIDSIMPL(IUpdater) {
 public:
  UpdaterImpl() = default;
  UpdaterImpl(const UpdaterImpl&) = delete;
  UpdaterImpl& operator=(const UpdaterImpl&) = delete;

  HRESULT RuntimeClassInitialize();

  // Overrides for IUpdater.
  IFACEMETHODIMP GetVersion(BSTR* version) override;
  IFACEMETHODIMP FetchPolicies(IUpdaterCallback* callback) override;

  // Returns `E_ACCESSDENIED` if the COM caller is not admin for a `system` app.
  IFACEMETHODIMP RegisterApp(const wchar_t* app_id,
                             const wchar_t* brand_code,
                             const wchar_t* brand_path,
                             const wchar_t* tag,
                             const wchar_t* version,
                             const wchar_t* existence_checker_path,
                             IUpdaterCallback* callback) override;
  IFACEMETHODIMP RunPeriodicTasks(IUpdaterCallback* callback) override;
  IFACEMETHODIMP CheckForUpdate(const wchar_t* app_id,
                                LONG priority,
                                BOOL same_version_update_allowed,
                                IUpdaterObserver* observer) override;
  IFACEMETHODIMP Update(const wchar_t* app_id,
                        const wchar_t* install_data_index,
                        LONG priority,
                        BOOL same_version_update_allowed,
                        IUpdaterObserver* observer) override;
  IFACEMETHODIMP UpdateAll(IUpdaterObserver* observer) override;

  // Returns `E_ACCESSDENIED` if the COM caller is not admin for a `system` app.
  IFACEMETHODIMP Install(const wchar_t* app_id,
                         const wchar_t* brand_code,
                         const wchar_t* brand_path,
                         const wchar_t* tag,
                         const wchar_t* version,
                         const wchar_t* existence_checker_path,
                         const wchar_t* client_install_data,
                         const wchar_t* install_data_index,
                         LONG priority,
                         IUpdaterObserver* observer) override;
  IFACEMETHODIMP CancelInstalls(const wchar_t* app_id) override;

  // Returns `E_ACCESSDENIED` if the COM caller is not admin for a `system` app.
  IFACEMETHODIMP RunInstaller(const wchar_t* app_id,
                              const wchar_t* installer_path,
                              const wchar_t* install_args,
                              const wchar_t* install_data,
                              const wchar_t* install_settings,
                              IUpdaterObserver* observer) override;
  IFACEMETHODIMP GetAppStates(IUpdaterAppStatesCallback* callback) override;

 private:
  ~UpdaterImpl() override = default;
};

// This class implements the IUpdaterInternal interface and exposes it as a COM
// object.
class UpdaterInternalImpl : public DYNAMICIIDSIMPL(IUpdaterInternal) {
 public:
  UpdaterInternalImpl() = default;
  UpdaterInternalImpl(const UpdaterInternalImpl&) = delete;
  UpdaterInternalImpl& operator=(const UpdaterInternalImpl&) = delete;

  HRESULT RuntimeClassInitialize();

  // Overrides for IUpdaterInternal.
  IFACEMETHODIMP Run(IUpdaterInternalCallback* callback) override;
  IFACEMETHODIMP Hello(IUpdaterInternalCallback* callback) override;

 private:
  ~UpdaterInternalImpl() override = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_H_
