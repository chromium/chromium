// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Definitions for native COM updater classes.

namespace updater {

// TODO(crbug.com/1065712): these classes do not have to be visible in the
// updater namespace. Additionally, there is some code duplication for the
// registration and unregistration code in both server and service_main
// compilation units.
//
// This class implements the IUpdateState interface and exposes it as a COM
// object. The purpose of this class is to remote the state of the
// |UpdateService|. Instances of this class are typically passed as arguments
// to RPC method calls which model COM events.
class UpdateStateImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdateState> {
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

 private:
  ~UpdateStateImpl() override = default;

  const UpdateService::UpdateState update_state_;
};

// This class implements the ICompleteStatus interface and exposes it as a COM
// object.
class CompleteStatusImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICompleteStatus> {
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
class UpdaterImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdater> {
 public:
  UpdaterImpl() = default;
  UpdaterImpl(const UpdaterImpl&) = delete;
  UpdaterImpl& operator=(const UpdaterImpl&) = delete;

  // Overrides for IUpdater.
  IFACEMETHODIMP GetVersion(BSTR* version) override;
  IFACEMETHODIMP CheckForUpdate(const wchar_t* app_id) override;
  IFACEMETHODIMP RegisterApp(const wchar_t* app_id,
                             const wchar_t* brand_code,
                             const wchar_t* tag,
                             const wchar_t* version,
                             const wchar_t* existence_checker_path,
                             IUpdaterRegisterAppCallback* callback) override;
  IFACEMETHODIMP RunPeriodicTasks(IUpdaterCallback* callback) override;
  IFACEMETHODIMP Update(const wchar_t* app_id,
                        IUpdaterObserver* observer) override;
  IFACEMETHODIMP UpdateAll(IUpdaterObserver* observer) override;

 private:
  ~UpdaterImpl() override = default;
};

// This class implements the IUpdaterInternal interface and exposes it as a COM
// object.
class UpdaterInternalImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdaterInternal> {
 public:
  UpdaterInternalImpl() = default;
  UpdaterInternalImpl(const UpdaterInternalImpl&) = delete;
  UpdaterInternalImpl& operator=(const UpdaterInternalImpl&) = delete;

  // Overrides for IUpdaterInternal.
  IFACEMETHODIMP Run(IUpdaterInternalCallback* callback) override;
  IFACEMETHODIMP InitializeUpdateService(
      IUpdaterInternalCallback* callback) override;

 private:
  ~UpdaterInternalImpl() override = default;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_COM_CLASSES_H_
