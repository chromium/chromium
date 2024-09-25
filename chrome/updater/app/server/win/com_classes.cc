// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes.h"

#include <wchar.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/variant_vector.h"
#include "chrome/updater/app/app_server_win.h"
#include "chrome/updater/app/server/win/com_classes_legacy.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/win_util.h"

namespace updater {
namespace {

// Maximum string length for COM strings.
constexpr size_t kMaxStringLen = 0x4000;  // 16KB.

using IUpdaterCallbackPtr = Microsoft::WRL::ComPtr<IUpdaterCallback>;
using IUpdaterInternalCallbackPtr =
    Microsoft::WRL::ComPtr<IUpdaterInternalCallback>;
using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;

// Implements `IUpdaterAppState`. Initialized with an `UpdateService::AppState`.
class UpdaterAppStateImpl : public IDispatchImpl<IUpdaterAppState> {
 public:
  UpdaterAppStateImpl()
      : IDispatchImpl<IUpdaterAppState>(IID_MAPS_USERSYSTEM(IUpdaterAppState)) {
  }
  UpdaterAppStateImpl(const UpdaterAppStateImpl&) = delete;
  UpdaterAppStateImpl& operator=(const UpdaterAppStateImpl&) = delete;

  HRESULT RuntimeClassInitialize(const UpdateService::AppState& app_state) {
    app_id_ = base::ASCIIToWide(app_state.app_id);
    version_ = base::ASCIIToWide(app_state.version.GetString());
    ap_ = base::ASCIIToWide(app_state.ap);
    brand_code_ = base::ASCIIToWide(app_state.brand_code);
    brand_path_ = app_state.brand_path.value();
    ecp_ = app_state.ecp.value();

    return S_OK;
  }

  IFACEMETHODIMP get_appId(BSTR* app_id) override {
    CHECK(app_id);

    *app_id = base::win::ScopedBstr(app_id_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_version(BSTR* version) override {
    CHECK(version);

    *version = base::win::ScopedBstr(version_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_ap(BSTR* ap) override {
    CHECK(ap);

    *ap = base::win::ScopedBstr(ap_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_brandCode(BSTR* brand_code) override {
    CHECK(brand_code);

    *brand_code = base::win::ScopedBstr(brand_code_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_brandPath(BSTR* brand_path) override {
    CHECK(brand_path);

    *brand_path = base::win::ScopedBstr(brand_path_).Release();
    return S_OK;
  }

  IFACEMETHODIMP get_ecp(BSTR* ecp) override {
    CHECK(ecp);

    *ecp = base::win::ScopedBstr(ecp_).Release();
    return S_OK;
  }

 private:
  ~UpdaterAppStateImpl() override = default;

  std::wstring app_id_;
  std::wstring version_;
  std::wstring ap_;
  std::wstring brand_code_;
  std::wstring brand_path_;
  std::wstring ecp_;
};

}  // namespace

STDMETHODIMP UpdateStateImpl::get_state(LONG* state) {
  CHECK(state);
  *state = static_cast<LONG>(update_state_.state);
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_appId(BSTR* app_id) {
  CHECK(app_id);
  *app_id =
      base::win::ScopedBstr(base::UTF8ToWide(update_state_.app_id)).Release();
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_nextVersion(BSTR* next_version) {
  CHECK(next_version);
  *next_version =
      base::win::ScopedBstr(
          update_state_.next_version.IsValid()
              ? base::UTF8ToWide(update_state_.next_version.GetString())
              : L"")
          .Release();
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_downloadedBytes(LONGLONG* downloaded_bytes) {
  CHECK(downloaded_bytes);
  *downloaded_bytes = LONGLONG{update_state_.downloaded_bytes};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_totalBytes(LONGLONG* total_bytes) {
  CHECK(total_bytes);
  *total_bytes = LONGLONG{update_state_.total_bytes};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_installProgress(LONG* install_progress) {
  CHECK(install_progress);
  *install_progress = LONG{update_state_.install_progress};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_errorCategory(LONG* error_category) {
  CHECK(error_category);
  *error_category = static_cast<LONG>(update_state_.error_category);
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_errorCode(LONG* error_code) {
  CHECK(error_code);
  *error_code = LONG{update_state_.error_code};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_extraCode1(LONG* extra_code1) {
  CHECK(extra_code1);
  *extra_code1 = LONG{update_state_.extra_code1};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_installerText(BSTR* installer_text) {
  CHECK(installer_text);
  *installer_text =
      base::win::ScopedBstr(base::UTF8ToWide(update_state_.installer_text))
          .Release();
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_installerCommandLine(
    BSTR* installer_cmd_line) {
  CHECK(installer_cmd_line);
  *installer_cmd_line =
      base::win::ScopedBstr(base::UTF8ToWide(update_state_.installer_cmd_line))
          .Release();
  return S_OK;
}

STDMETHODIMP CompleteStatusImpl::get_statusCode(LONG* code) {
  CHECK(code);
  *code = code_;
  return S_OK;
}

STDMETHODIMP CompleteStatusImpl::get_statusMessage(BSTR* message) {
  CHECK(message);
  *message = base::win::ScopedBstr(message_).Release();
  return S_OK;
}

HRESULT UpdaterImpl::RuntimeClassInitialize() {
  return S_OK;
}

HRESULT UpdaterImpl::GetVersion(BSTR* version) {
  CHECK(version);

  // Return the hardcoded version instead of calling the corresponding
  // non-blocking function of `UpdateServiceImpl`. This results in some
  // code duplication but it avoids the complexities of making this function
  // non-blocking.
  *version = base::win::ScopedBstr(kUpdaterVersionUtf16).Release();
  return S_OK;
}

HRESULT UpdaterImpl::FetchPolicies(IUpdaterCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }

  base::OnceCallback<void(int)> updater_callback = base::BindPostTask(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      base::BindOnce(
          [](IUpdaterCallbackPtr callback, int result) {
            HRESULT hr = callback->Run(result);
            VLOG(2) << "IUpdaterImpl::FetchPolicies. "
                    << "IUpdaterCallback::Run returned " << std::hex << hr;
          },
          IUpdaterCallbackPtr(callback)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](base::OnceCallback<void(int)> updater_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(updater_callback).Run(-1);
          return;
        }
        update_service->FetchPolicies(std::move(updater_callback));
      },
      std::move(updater_callback)));
  return S_OK;
}

HRESULT UpdaterImpl::RegisterApp(const wchar_t* app_id,
                                 const wchar_t* brand_code,
                                 const wchar_t* brand_path,
                                 const wchar_t* ap,
                                 const wchar_t* version,
                                 const wchar_t* existence_checker_path,
                                 IUpdaterCallback* callback) {
  if (FAILED(IsCOMCallerAllowed())) {
    return E_ACCESSDENIED;
  }

  if (!callback) {
    return E_INVALIDARG;
  }

  // Validates that string parameters are not longer than 16K characters.
  std::optional<RegistrationRequest> request =
      [app_id, brand_code, brand_path, ap, version,
       existence_checker_path]() -> decltype(request) {
    for (const auto& str : {app_id, brand_code, brand_path, ap, version,
                            existence_checker_path}) {
      if (wcsnlen_s(str, kMaxStringLen) == kMaxStringLen) {
        return std::nullopt;
      }
    }

    RegistrationRequest request;
    if (!app_id || !base::WideToUTF8(app_id, wcslen(app_id), &request.app_id)) {
      return std::nullopt;
    }
    if (!brand_code || !base::WideToUTF8(brand_code, wcslen(brand_code),
                                         &request.brand_code)) {
      return std::nullopt;
    }
    request.brand_path = base::FilePath(brand_path);
    if (!ap || !base::WideToUTF8(ap, wcslen(ap), &request.ap)) {
      return std::nullopt;
    }
    std::string version_str;
    if (!version || !base::WideToUTF8(version, wcslen(version), &version_str)) {
      return std::nullopt;
    }
    request.version = base::Version(version_str);
    if (!request.version.IsValid()) {
      return std::nullopt;
    }
    request.existence_checker_path = base::FilePath(existence_checker_path);

    return request;
  }();

  if (!request) {
    return E_INVALIDARG;
  }

  base::OnceCallback<void(int)> updater_callback = base::BindPostTask(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      base::BindOnce(
          [](IUpdaterCallbackPtr callback, int result) {
            HRESULT hr = callback->Run(result);
            VLOG(2) << "IUpdaterImpl::RegisterApp. "
                    << "IUpdaterCallback::Run returned " << std::hex << hr;
          },
          Microsoft::WRL::ComPtr<IUpdaterCallback>(callback)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](const RegistrationRequest& request,
         base::OnceCallback<void(int)> updater_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(updater_callback).Run(-1);
          return;
        }
        update_service->RegisterApp(request, std::move(updater_callback));
      },
      *request, std::move(updater_callback)));
  return S_OK;
}

// Called by the COM RPC runtime on one of its threads. Invokes the in-process
// `update_service` on the main sequence. The callbacks received from
// `update_service` arrive in the main sequence too.
HRESULT UpdaterImpl::RunPeriodicTasks(IUpdaterCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }
  AppServerWin::PostRpcTask(base::BindOnce(
      [](base::OnceClosure callback_closure) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(callback_closure).Run();
          return;
        }
        update_service->RunPeriodicTasks(std::move(callback_closure));
      },
      base::BindPostTask(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          base::BindOnce(base::IgnoreResult(&IUpdaterCallback::Run),
                         Microsoft::WRL::ComPtr<IUpdaterCallback>(callback),
                         0))));
  return S_OK;
}

namespace {

// Filters the download progress events to avoid spamming the RPC client
// with too many download progress notifications. The filter only notifies
// the client at most once for every unit of download progress made.
//
// The instance of this class is owned by the repeating callback which is
// invoking `OnStateChange`.
class StateChangeCallbackFilter {
 public:
  StateChangeCallbackFilter(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      Microsoft::WRL::ComPtr<IUpdaterObserver> observer)
      : task_runner_(task_runner), observer_(observer) {
    CHECK(observer);
  }
  StateChangeCallbackFilter(const StateChangeCallbackFilter&) = delete;
  StateChangeCallbackFilter& operator=(const StateChangeCallbackFilter&) =
      delete;

  void OnStateChange(const UpdateService::UpdateState& update_state) {
    int cur_progress = GetDownloadProgress(update_state.downloaded_bytes,
                                           update_state.total_bytes);
    if (update_state.state == UpdateService::UpdateState::State::kDownloading &&
        progress_seen_ && *progress_seen_ == cur_progress) {
      return;
    }
    progress_seen_ = cur_progress;
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&IUpdaterObserver::OnStateChange, observer_,
                       MakeComObjectOrCrash<UpdateStateImpl>(update_state)),
        base::BindOnce([](HRESULT hr) {
          VLOG(4) << "IUpdaterObserver::OnStateChange returned " << std::hex
                  << hr;
        }));
  }

 private:
  // Calls the COM function IUpdaterObserver::OnStateChange on `observer_`.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Microsoft::WRL::ComPtr<IUpdaterObserver> observer_;

  // Most recent download progress value the client has been notified about.
  std::optional<int> progress_seen_;
};

}  // namespace

HRESULT UpdaterImpl::CheckForUpdate(const wchar_t* app_id,
                                    LONG priority,
                                    BOOL same_version_update_allowed,
                                    IUpdaterObserver* observer) {
  if (!observer) {
    return E_INVALIDARG;
  }

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  base::RepeatingCallback<void(const UpdateService::UpdateState&)>
      state_change_callback = base::BindRepeating(
          &StateChangeCallbackFilter::OnStateChange,
          base::Owned(new StateChangeCallbackFilter(task_runner, observer)));
  base::OnceCallback<void(UpdateService::Result)> complete_callback =
      base::BindPostTask(
          task_runner,
          base::BindOnce(
              [](IUpdaterObserverPtr observer, UpdateService::Result result) {
                HRESULT hr = observer->OnComplete(
                    MakeComObjectOrCrash<CompleteStatusImpl>(
                        static_cast<int>(result), L"")
                        .Get());
                VLOG(2) << "IUpdaterImpl::CheckForUpdate. "
                        << "IUpdaterObserver::OnComplete returned " << std::hex
                        << hr;
              },
              IUpdaterObserverPtr(observer)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](const std::string& app_id, UpdateService::Priority priority,
         bool same_version_update_allowed,
         base::RepeatingCallback<void(const UpdateService::UpdateState&)>
             state_change_callback,
         base::OnceCallback<void(UpdateService::Result)> complete_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(complete_callback)
              .Run(UpdateService::Result::kServiceStopped);
          return;
        }
        update_service->CheckForUpdate(
            app_id, priority,
            same_version_update_allowed
                ? UpdateService::PolicySameVersionUpdate::kAllowed
                : UpdateService::PolicySameVersionUpdate::kNotAllowed,
            std::move(state_change_callback), std::move(complete_callback));
      },
      base::WideToUTF8(app_id), static_cast<UpdateService::Priority>(priority),
      same_version_update_allowed, std::move(state_change_callback),
      std::move(complete_callback)));
  return S_OK;
}

// Called by the COM RPC runtime on one of its threads. Invokes the in-process
// `update_service` on the main sequence. The callbacks received from
// `update_service` arrive in the main sequence too. Since handling these
// callbacks involves issuing outgoing COM RPC calls, which block, such COM
// calls must be done through a task runner, bound to the closures provided
// as parameters for the UpdateService::Update call.
HRESULT UpdaterImpl::Update(const wchar_t* app_id,
                            const wchar_t* install_data_index,
                            LONG priority,
                            BOOL same_version_update_allowed,
                            IUpdaterObserver* observer) {
  if (!observer) {
    return E_INVALIDARG;
  }

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  base::RepeatingCallback<void(const UpdateService::UpdateState&)>
      state_change_callback = base::BindRepeating(
          &StateChangeCallbackFilter::OnStateChange,
          base::Owned(new StateChangeCallbackFilter(task_runner, observer)));
  base::OnceCallback<void(UpdateService::Result)> complete_callback =
      base::BindPostTask(
          task_runner,
          base::BindOnce(
              [](IUpdaterObserverPtr observer, UpdateService::Result result) {
                HRESULT hr = observer->OnComplete(
                    MakeComObjectOrCrash<CompleteStatusImpl>(
                        static_cast<int>(result), L"")
                        .Get());
                VLOG(2) << "IUpdaterImpl::Update. "
                        << "IUpdaterObserver::OnComplete returned " << std::hex
                        << hr;
              },
              IUpdaterObserverPtr(observer)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](const std::string& app_id, const std::string& install_data_index,
         UpdateService::Priority priority, bool same_version_update_allowed,
         base::RepeatingCallback<void(const UpdateService::UpdateState&)>
             state_change_callback,
         base::OnceCallback<void(UpdateService::Result)> complete_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(complete_callback)
              .Run(UpdateService::Result::kServiceStopped);
          return;
        }
        update_service->Update(
            app_id, install_data_index, priority,
            same_version_update_allowed
                ? UpdateService::PolicySameVersionUpdate::kAllowed
                : UpdateService::PolicySameVersionUpdate::kNotAllowed,
            std::move(state_change_callback), std::move(complete_callback));
      },
      base::WideToUTF8(app_id), base::WideToUTF8(install_data_index),
      static_cast<UpdateService::Priority>(priority),
      same_version_update_allowed, std::move(state_change_callback),
      std::move(complete_callback)));
  return S_OK;
}

// See the comment for the UpdaterImpl::Update.
HRESULT UpdaterImpl::UpdateAll(IUpdaterObserver* observer) {
  if (!observer) {
    return E_INVALIDARG;
  }

  base::OnceCallback<void(UpdateService::Result)> complete_callback =
      base::BindPostTask(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          base::BindOnce(
              [](IUpdaterObserverPtr observer, UpdateService::Result result) {
                HRESULT hr = observer->OnComplete(
                    MakeComObjectOrCrash<CompleteStatusImpl>(
                        static_cast<int>(result), L"")
                        .Get());
                VLOG(2) << "IUpdaterImpl::UpdateAll. "
                        << "IUpdaterObserver::OnComplete returned " << std::hex
                        << hr;
              },
              IUpdaterObserverPtr(observer)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](base::OnceCallback<void(UpdateService::Result)> complete_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(complete_callback)
              .Run(UpdateService::Result::kServiceStopped);
          return;
        }
        update_service->UpdateAll(base::DoNothing(),
                                  std::move(complete_callback));
      },
      std::move(complete_callback)));
  return S_OK;
}

HRESULT UpdaterImpl::Install(const wchar_t* app_id,
                             const wchar_t* brand_code,
                             const wchar_t* brand_path,
                             const wchar_t* ap,
                             const wchar_t* version,
                             const wchar_t* existence_checker_path,
                             const wchar_t* client_install_data,
                             const wchar_t* install_data_index,
                             LONG priority,
                             IUpdaterObserver* observer) {
  if (FAILED(IsCOMCallerAllowed())) {
    return E_ACCESSDENIED;
  }

  if (!observer) {
    return E_INVALIDARG;
  }

  // Validates that string parameters are not longer than 16K characters.
  std::optional<RegistrationRequest> request =
      [app_id, brand_code, brand_path, ap, version, existence_checker_path,
       client_install_data, install_data_index]() -> decltype(request) {
    for (const auto& str :
         {app_id, brand_code, brand_path, ap, version, existence_checker_path,
          client_install_data, install_data_index}) {
      if (wcsnlen_s(str, kMaxStringLen) == kMaxStringLen) {
        return std::nullopt;
      }
    }

    RegistrationRequest request;
    if (!app_id || !base::WideToUTF8(app_id, wcslen(app_id), &request.app_id)) {
      return std::nullopt;
    }
    if (!brand_code || !base::WideToUTF8(brand_code, wcslen(brand_code),
                                         &request.brand_code)) {
      return std::nullopt;
    }
    request.brand_path = base::FilePath(brand_path);
    if (!ap || !base::WideToUTF8(ap, wcslen(ap), &request.ap)) {
      return std::nullopt;
    }
    std::string version_str;
    if (!version || !base::WideToUTF8(version, wcslen(version), &version_str)) {
      return std::nullopt;
    }
    request.version = base::Version(version_str);
    if (!request.version.IsValid()) {
      return std::nullopt;
    }
    request.existence_checker_path = base::FilePath(existence_checker_path);

    return request;
  }();

  if (!request) {
    return E_INVALIDARG;
  }

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  base::RepeatingCallback<void(const UpdateService::UpdateState&)>
      state_change_callback = base::BindRepeating(
          &StateChangeCallbackFilter::OnStateChange,
          base::Owned(new StateChangeCallbackFilter(task_runner, observer)));
  base::OnceCallback<void(UpdateService::Result)> complete_callback =
      base::BindPostTask(
          task_runner,
          base::BindOnce(
              [](IUpdaterObserverPtr observer, UpdateService::Result result) {
                HRESULT hr = observer->OnComplete(
                    MakeComObjectOrCrash<CompleteStatusImpl>(
                        static_cast<int>(result), L"")
                        .Get());
                VLOG(2) << "IUpdaterImpl::Install. "
                        << "IUpdaterObserver::OnComplete returned " << std::hex
                        << hr;
              },
              IUpdaterObserverPtr(observer)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](const RegistrationRequest& request,
         const std::string& client_install_data,
         const std::string& install_data_index,
         UpdateService::Priority priority,
         base::RepeatingCallback<void(const UpdateService::UpdateState&)>
             state_change_callback,
         base::OnceCallback<void(UpdateService::Result)> complete_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(complete_callback)
              .Run(UpdateService::Result::kServiceStopped);
          return;
        }
        update_service->Install(
            request, client_install_data, install_data_index, priority,
            std::move(state_change_callback), std::move(complete_callback));
      },
      *request, base::WideToUTF8(client_install_data),
      base::WideToUTF8(install_data_index),
      static_cast<UpdateService::Priority>(priority),
      std::move(state_change_callback), std::move(complete_callback)));
  return S_OK;
}

HRESULT UpdaterImpl::CancelInstalls(const wchar_t* app_id) {
  std::string app_id_str;
  if (wcsnlen_s(app_id, kMaxStringLen) >= kMaxStringLen || !app_id ||
      !base::WideToUTF8(app_id, wcslen(app_id), &app_id_str)) {
    return E_INVALIDARG;
  }
  AppServerWin::PostRpcTask(base::BindOnce(
      [](const std::string& app_id_str) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          return;
        }
        update_service->CancelInstalls(app_id_str);
      },
      app_id_str));
  return S_OK;
}

HRESULT UpdaterImpl::RunInstaller(const wchar_t* app_id,
                                  const wchar_t* installer_path,
                                  const wchar_t* install_args,
                                  const wchar_t* install_data,
                                  const wchar_t* install_settings,
                                  IUpdaterObserver* observer) {
  if (FAILED(IsCOMCallerAllowed())) {
    return E_ACCESSDENIED;
  }

  VLOG(1) << __func__;

  if (!observer) {
    return E_INVALIDARG;
  }

  for (const wchar_t* str :
       {app_id, installer_path, install_args, install_data, install_settings}) {
    if (wcsnlen_s(str, kMaxStringLen) >= kMaxStringLen) {
      return E_INVALIDARG;
    }
  }

  std::string app_id_str;
  if (!app_id || !base::WideToUTF8(app_id, wcslen(app_id), &app_id_str)) {
    return E_INVALIDARG;
  }

  if (!installer_path) {
    return E_INVALIDARG;
  }

  std::string install_args_str;
  if (install_args && !base::WideToUTF8(install_args, wcslen(install_args),
                                        &install_args_str)) {
    return E_INVALIDARG;
  }

  std::string install_settings_str;
  if (install_settings &&
      !base::WideToUTF8(install_settings, wcslen(install_settings),
                        &install_settings_str)) {
    return E_INVALIDARG;
  }

  std::string install_data_str;
  if (install_data && !base::WideToUTF8(install_data, wcslen(install_data),
                                        &install_data_str)) {
    return E_INVALIDARG;
  }

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  base::RepeatingCallback<void(const UpdateService::UpdateState&)>
      state_change_callback = base::BindRepeating(
          &StateChangeCallbackFilter::OnStateChange,
          base::Owned(new StateChangeCallbackFilter(task_runner, observer)));
  base::OnceCallback<void(UpdateService::Result)> complete_callback =
      base::BindPostTask(
          task_runner,
          base::BindOnce(
              [](IUpdaterObserverPtr observer, UpdateService::Result result) {
                HRESULT hr = observer->OnComplete(
                    MakeComObjectOrCrash<CompleteStatusImpl>(
                        static_cast<int>(result), L"")
                        .Get());
                VLOG(2) << "IUpdaterImpl::RunInstaller. "
                        << "IUpdaterObserver::OnComplete returned " << std::hex
                        << hr;
              },
              IUpdaterObserverPtr(observer)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](const std::string& app_id, const base::FilePath& installer_path,
         const std::string& install_args, const std::string& install_data,
         const std::string& install_settings,
         base::RepeatingCallback<void(const UpdateService::UpdateState&)>
             state_change_callback,
         base::OnceCallback<void(UpdateService::Result)> complete_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(complete_callback)
              .Run(UpdateService::Result::kServiceStopped);
          return;
        }
        update_service->RunInstaller(app_id, installer_path, install_args,
                                     install_data, install_settings,
                                     std::move(state_change_callback),
                                     std::move(complete_callback));
      },
      app_id_str, base::FilePath(installer_path), install_args_str,
      install_data_str, install_settings_str, std::move(state_change_callback),
      std::move(complete_callback)));
  return S_OK;
}

HRESULT UpdaterImpl::GetAppStates(IUpdaterAppStatesCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }

  base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
      get_app_states_callback = base::BindPostTask(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          base::BindOnce(
              [](Microsoft::WRL::ComPtr<IUpdaterAppStatesCallback> callback,
                 const std::vector<UpdateService::AppState>& app_states) {
                // Converts `app_states` into a `SAFEARRAY` of `IDispatch`
                // and calls `IUpdaterAppStatesCallback::Run` with the
                // resulting `VARIANT`.
                base::win::VariantVector updater_app_states;
                for (const auto& app_state : app_states) {
                  Microsoft::WRL::ComPtr<IDispatch> dispatch;
                  CHECK(
                      SUCCEEDED(MakeAndInitializeComObject<UpdaterAppStateImpl>(
                          dispatch, app_state)));
                  updater_app_states.Insert<VT_DISPATCH>(dispatch.Get());
                }
                base::win::ScopedVariant variant;
                variant.Reset(updater_app_states.ReleaseAsSafearrayVariant());
                callback->Run(variant);
              },
              Microsoft::WRL::ComPtr<IUpdaterAppStatesCallback>(callback)));
  AppServerWin::PostRpcTask(base::BindOnce(
      [](base::OnceCallback<void(const std::vector<UpdateService::AppState>&)>
             get_app_states_callback) {
        scoped_refptr<UpdateService> update_service =
            GetAppServerWinInstance()->update_service();
        if (!update_service) {
          std::move(get_app_states_callback)
              .Run(std::vector<UpdateService::AppState>());
          return;
        }
        update_service->GetAppStates(std::move(get_app_states_callback));
      },
      std::move(get_app_states_callback)));
  return S_OK;
}

HRESULT UpdaterInternalImpl::RuntimeClassInitialize() {
  return S_OK;
}

// See the comment for the UpdaterImpl::Update.
HRESULT UpdaterInternalImpl::Run(IUpdaterInternalCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }

  base::OnceClosure updater_internal_callback = base::BindPostTask(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      base::BindOnce(
          [](IUpdaterInternalCallbackPtr callback) {
            HRESULT hr = callback->Run(0);
            VLOG(2) << "UpdaterInternalImpl::Run. "
                    << "IUpdaterInternalCallback::Run returned " << std::hex
                    << hr;
          },
          IUpdaterInternalCallbackPtr(callback)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](base::OnceClosure updater_internal_callback) {
        scoped_refptr<UpdateServiceInternal> update_service_internal =
            GetAppServerWinInstance()->update_service_internal();
        if (!update_service_internal) {
          std::move(updater_internal_callback).Run();
          return;
        }
        update_service_internal->Run(std::move(updater_internal_callback));
      },
      std::move(updater_internal_callback)));
  return S_OK;
}

HRESULT UpdaterInternalImpl::Hello(IUpdaterInternalCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }

  base::OnceClosure updater_internal_callback = base::BindPostTask(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
      base::BindOnce(
          [](IUpdaterInternalCallbackPtr callback) {
            HRESULT hr = callback->Run(0);
            VLOG(2) << "UpdaterInternalImpl::Hello. "
                    << "IUpdaterInternalCallback::Run returned " << std::hex
                    << hr;
          },
          IUpdaterInternalCallbackPtr(callback)));

  AppServerWin::PostRpcTask(base::BindOnce(
      [](base::OnceClosure updater_internal_callback) {
        scoped_refptr<UpdateServiceInternal> update_service_internal =
            GetAppServerWinInstance()->update_service_internal();
        if (!update_service_internal) {
          std::move(updater_internal_callback).Run();
          return;
        }
        update_service_internal->Hello(std::move(updater_internal_callback));
      },
      std::move(updater_internal_callback)));
  return S_OK;
}

}  // namespace updater
