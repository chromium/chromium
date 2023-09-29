// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes.h"

#include <wchar.h>
#include <wrl/client.h>
#include <wrl/implements.h>

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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

// Maximum string length for COM strings.
constexpr size_t kMaxStringLen = 0x4000;  // 16KB.

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
  HRESULT hr = IsCOMCallerAllowed();
  if (FAILED(hr)) {
    return hr;
  }

  // TODO(crbug.com/1484803): remove once we know why E_NOINTERFACE happens.
  return GetAppServerWinInstance()->RestoreComInterfaces(false) ? S_OK
                                                                : E_UNEXPECTED;
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
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             base::OnceCallback<void(int)> result_callback) {
            update_service->FetchPolicies(std::move(result_callback));
          },
          com_server->update_service(),
          base::BindPostTask(
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::MayBlock(),
                   base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
              base::BindOnce(
                  [](Microsoft::WRL::ComPtr<IUpdaterCallback> callback,
                     int result) { callback->Run(result); },
                  Microsoft::WRL::ComPtr<IUpdaterCallback>(callback)))));
  return S_OK;
}

HRESULT UpdaterImpl::RegisterApp(const wchar_t* app_id,
                                 const wchar_t* brand_code,
                                 const wchar_t* brand_path,
                                 const wchar_t* ap,
                                 const wchar_t* version,
                                 const wchar_t* existence_checker_path,
                                 IUpdaterCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }

  // Validates that string parameters are not longer than 16K characters.
  absl::optional<RegistrationRequest> request =
      [app_id, brand_code, brand_path, ap, version,
       existence_checker_path]() -> decltype(request) {
    for (const auto* str : {app_id, brand_code, brand_path, ap, version,
                            existence_checker_path}) {
      if (wcsnlen_s(str, kMaxStringLen) == kMaxStringLen) {
        return absl::nullopt;
      }
    }

    RegistrationRequest request;
    if (!app_id || !base::WideToUTF8(app_id, wcslen(app_id), &request.app_id)) {
      return absl::nullopt;
    }
    if (!brand_code || !base::WideToUTF8(brand_code, wcslen(brand_code),
                                         &request.brand_code)) {
      return absl::nullopt;
    }
    request.brand_path = base::FilePath(brand_path);
    if (!ap || !base::WideToUTF8(ap, wcslen(ap), &request.ap)) {
      return absl::nullopt;
    }
    std::string version_str;
    if (!version || !base::WideToUTF8(version, wcslen(version), &version_str)) {
      return absl::nullopt;
    }
    request.version = base::Version(version_str);
    if (!request.version.IsValid()) {
      return absl::nullopt;
    }
    request.existence_checker_path = base::FilePath(existence_checker_path);

    return request;
  }();

  if (!request)
    return E_INVALIDARG;

  using IUpdaterCallbackPtr = Microsoft::WRL::ComPtr<IUpdaterCallback>;
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();

  // This task runner is responsible for sequencing the COM calls and callbacks.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const RegistrationRequest& request, IUpdaterCallbackPtr callback) {
            update_service->RegisterApp(
                request,
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterCallbackPtr callback, int result) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(&IUpdaterCallback::Run, callback,
                                         result),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(2) << "UpdaterImpl::RegisterApp "
                                    << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, callback));
          },
          com_server->update_service(), task_runner, *request,
          IUpdaterCallbackPtr(callback)));

  return S_OK;
}

// Called by the COM RPC runtime on one of its threads. Invokes the in-process
// `update_service` on the main sequence. The callbacks received from
// `update_service` arrive in the main sequence too.
HRESULT UpdaterImpl::RunPeriodicTasks(IUpdaterCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             base::OnceClosure callback_closure) {
            update_service->RunPeriodicTasks(std::move(callback_closure));
          },
          com_server->update_service(),
          base::BindPostTask(
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::MayBlock(),
                   base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
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
  absl::optional<int> progress_seen_;
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
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;
  auto observer_local = IUpdaterObserverPtr(observer);

  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const std::string& app_id, UpdateService::Priority priority,
             bool same_version_update_allowed, IUpdaterObserverPtr observer) {
            update_service->CheckForUpdate(
                app_id, priority,
                same_version_update_allowed
                    ? UpdateService::PolicySameVersionUpdate::kAllowed
                    : UpdateService::PolicySameVersionUpdate::kNotAllowed,
                base::BindRepeating(&StateChangeCallbackFilter::OnStateChange,
                                    base::Owned(new StateChangeCallbackFilter(
                                        task_runner, observer))),
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterObserverPtr observer,
                       UpdateService::Result result) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              MakeComObjectOrCrash<CompleteStatusImpl>(
                                  static_cast<int>(result), L"")),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(2) << "UpdaterImpl::Update "
                                    << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, observer));
          },
          com_server->update_service(), task_runner, base::WideToUTF8(app_id),
          static_cast<UpdateService::Priority>(priority),
          same_version_update_allowed, observer_local));
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

  // This task runner is responsible for sequencing the callbacks posted
  // by the `UpdateService` and calling the outbound COM functions to
  // notify the client about state changes in the `UpdateService`.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;
  auto observer_local = IUpdaterObserverPtr(observer);

  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const std::string& app_id, const std::string& install_data_index,
             UpdateService::Priority priority, bool same_version_update_allowed,
             IUpdaterObserverPtr observer) {
            update_service->Update(
                app_id, install_data_index, priority,
                same_version_update_allowed
                    ? UpdateService::PolicySameVersionUpdate::kAllowed
                    : UpdateService::PolicySameVersionUpdate::kNotAllowed,
                base::BindRepeating(&StateChangeCallbackFilter::OnStateChange,
                                    base::Owned(new StateChangeCallbackFilter(
                                        task_runner, observer))),
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterObserverPtr observer,
                       UpdateService::Result result) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              MakeComObjectOrCrash<CompleteStatusImpl>(
                                  static_cast<int>(result), L"")),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(2) << "UpdaterImpl::Update "
                                    << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, observer));
          },
          com_server->update_service(), task_runner, base::WideToUTF8(app_id),
          base::WideToUTF8(install_data_index),
          static_cast<UpdateService::Priority>(priority),
          same_version_update_allowed, observer_local));
  return S_OK;
}

// See the comment for the UpdaterImpl::Update.
HRESULT UpdaterImpl::UpdateAll(IUpdaterObserver* observer) {
  if (!observer) {
    return E_INVALIDARG;
  }

  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             IUpdaterObserverPtr observer) {
            update_service->UpdateAll(
                base::DoNothing(),
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterObserverPtr observer,
                       UpdateService::Result result) {
                      // The COM RPC outgoing call blocks and it must be posted
                      // through the thread pool.
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              MakeComObjectOrCrash<CompleteStatusImpl>(
                                  static_cast<int>(result), L"")),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(2) << "UpdaterImpl::UpdateAll "
                                    << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, observer));
          },
          com_server->update_service(), task_runner,
          IUpdaterObserverPtr(observer)));
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
  if (!observer) {
    return E_INVALIDARG;
  }

  // Validates that string parameters are not longer than 16K characters.
  absl::optional<RegistrationRequest> request =
      [app_id, brand_code, brand_path, ap, version, existence_checker_path,
       client_install_data, install_data_index]() -> decltype(request) {
    for (const auto* str :
         {app_id, brand_code, brand_path, ap, version, existence_checker_path,
          client_install_data, install_data_index}) {
      if (wcsnlen_s(str, kMaxStringLen) == kMaxStringLen) {
        return absl::nullopt;
      }
    }

    RegistrationRequest request;
    if (!app_id || !base::WideToUTF8(app_id, wcslen(app_id), &request.app_id)) {
      return absl::nullopt;
    }
    if (!brand_code || !base::WideToUTF8(brand_code, wcslen(brand_code),
                                         &request.brand_code)) {
      return absl::nullopt;
    }
    request.brand_path = base::FilePath(brand_path);
    if (!ap || !base::WideToUTF8(ap, wcslen(ap), &request.ap)) {
      return absl::nullopt;
    }
    std::string version_str;
    if (!version || !base::WideToUTF8(version, wcslen(version), &version_str)) {
      return absl::nullopt;
    }
    request.version = base::Version(version_str);
    if (!request.version.IsValid()) {
      return absl::nullopt;
    }
    request.existence_checker_path = base::FilePath(existence_checker_path);

    return request;
  }();

  if (!request)
    return E_INVALIDARG;

  // This task runner is responsible for sequencing the callbacks posted
  // by the `UpdateService` and calling the outbound COM functions to
  // notify the client about state changes in the `UpdateService`.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;
  auto observer_local = IUpdaterObserverPtr(observer);

  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const RegistrationRequest& request,
             const std::string& client_install_data,
             const std::string& install_data_index,
             UpdateService::Priority priority, IUpdaterObserverPtr observer) {
            update_service->Install(
                request, client_install_data, install_data_index, priority,
                base::BindRepeating(&StateChangeCallbackFilter::OnStateChange,
                                    base::Owned(new StateChangeCallbackFilter(
                                        task_runner, observer))),
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterObserverPtr observer,
                       UpdateService::Result result) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              MakeComObjectOrCrash<CompleteStatusImpl>(
                                  static_cast<int>(result), L"")),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(1) << "UpdaterImpl::Install "
                                    << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, observer));
          },
          com_server->update_service(), task_runner, *request,
          base::WideToUTF8(client_install_data),
          base::WideToUTF8(install_data_index),
          static_cast<UpdateService::Priority>(priority), observer_local));
  return S_OK;
}

HRESULT UpdaterImpl::CancelInstalls(const wchar_t* app_id) {
  std::string app_id_str;
  if (wcsnlen_s(app_id, kMaxStringLen) >= kMaxStringLen || !app_id ||
      !base::WideToUTF8(app_id, wcslen(app_id), &app_id_str)) {
    return E_INVALIDARG;
  }

  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UpdateService::CancelInstalls,
                                com_server->update_service(), app_id_str));

  return S_OK;
}

HRESULT UpdaterImpl::RunInstaller(const wchar_t* app_id,
                                  const wchar_t* installer_path,
                                  const wchar_t* install_args,
                                  const wchar_t* install_data,
                                  const wchar_t* install_settings,
                                  IUpdaterObserver* observer) {
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

  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();

  // This task runner is responsible for sequencing the COM calls and callbacks.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const std::string& app_id, const base::FilePath& installer_path,
             const std::string& install_args, const std::string& install_data,
             const std::string& install_settings,
             IUpdaterObserverPtr observer) {
            update_service->RunInstaller(
                app_id, installer_path, install_args, install_data,
                install_settings,
                base::BindRepeating(&StateChangeCallbackFilter::OnStateChange,
                                    base::Owned(new StateChangeCallbackFilter(
                                        task_runner, observer))),
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterObserverPtr observer,
                       const UpdateService::Result result) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              MakeComObjectOrCrash<CompleteStatusImpl>(
                                  static_cast<int>(result), L"")),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(2) << "UpdaterImpl::RunInstaller "
                                    << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, observer));
          },
          com_server->update_service(), task_runner, app_id_str,
          base::FilePath(installer_path), install_args_str, install_data_str,
          install_settings_str, IUpdaterObserverPtr(observer)));

  return S_OK;
}

HRESULT UpdaterImpl::GetAppStates(IUpdaterAppStatesCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             base::OnceCallback<void(
                 const std::vector<UpdateService::AppState>&)>
                 result_callback) {
            update_service->GetAppStates(std::move(result_callback));
          },
          com_server->update_service(),
          base::BindPostTask(
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::MayBlock(),
                   base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
              base::BindOnce(
                  [](Microsoft::WRL::ComPtr<IUpdaterAppStatesCallback> callback,
                     const std::vector<UpdateService::AppState>& app_states) {
                    // Converts `app_states` into a `SAFEARRAY` of `IDispatch`
                    // and calls `IUpdaterAppStatesCallback::Run` with the
                    // resulting `VARIANT`.
                    base::win::VariantVector updater_app_states;

                    for (const auto& app_state : app_states) {
                      Microsoft::WRL::ComPtr<IDispatch> dispatch;
                      CHECK(SUCCEEDED(
                          MakeAndInitializeComObject<UpdaterAppStateImpl>(
                              dispatch, app_state)));

                      updater_app_states.Insert<VT_DISPATCH>(dispatch.Get());
                    }

                    base::win::ScopedVariant variant;
                    variant.Reset(
                        updater_app_states.ReleaseAsSafearrayVariant());
                    callback->Run(variant);
                  },
                  Microsoft::WRL::ComPtr<IUpdaterAppStatesCallback>(
                      callback)))));
  return S_OK;
}

HRESULT UpdaterInternalImpl::RuntimeClassInitialize() {
  HRESULT hr = IsCOMCallerAllowed();
  if (FAILED(hr)) {
    return hr;
  }

  // TODO(crbug.com/1484803): remove once we know why E_NOINTERFACE happens.
  return GetAppServerWinInstance()->RestoreComInterfaces(true) ? S_OK
                                                               : E_UNEXPECTED;
}

// See the comment for the UpdaterImpl::Update.
HRESULT UpdaterInternalImpl::Run(IUpdaterInternalCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }

  using IUpdaterInternalCallbackPtr =
      Microsoft::WRL::ComPtr<IUpdaterInternalCallback>;
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateServiceInternal> update_service_internal,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             IUpdaterInternalCallbackPtr callback) {
            update_service_internal->Run(base::BindOnce(
                [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                   IUpdaterInternalCallbackPtr callback) {
                  task_runner->PostTaskAndReplyWithResult(
                      FROM_HERE,
                      base::BindOnce(&IUpdaterInternalCallback::Run, callback,
                                     0),
                      base::BindOnce([](HRESULT hr) {
                        VLOG(2) << "UpdaterInternalImpl::Run "
                                << "callback returned " << std::hex << hr;
                      }));
                },
                task_runner, callback));
          },
          com_server->update_service_internal(), task_runner,
          IUpdaterInternalCallbackPtr(callback)));
  return S_OK;
}

HRESULT UpdaterInternalImpl::Hello(IUpdaterInternalCallback* callback) {
  if (!callback) {
    return E_INVALIDARG;
  }

  using IUpdaterInternalCallbackPtr =
      Microsoft::WRL::ComPtr<IUpdaterInternalCallback>;
  scoped_refptr<AppServerWin> com_server = GetAppServerWinInstance();

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateServiceInternal> update_service_internal,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             IUpdaterInternalCallbackPtr callback) {
            update_service_internal->Hello(base::BindOnce(
                [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                   IUpdaterInternalCallbackPtr callback) {
                  task_runner->PostTaskAndReplyWithResult(
                      FROM_HERE,
                      base::BindOnce(&IUpdaterInternalCallback::Run, callback,
                                     0),
                      base::BindOnce([](HRESULT hr) {
                        VLOG(2) << "UpdaterInternalImpl::Hello "
                                << "callback returned " << std::hex << hr;
                      }));
                },
                task_runner, callback));
          },
          com_server->update_service_internal(), task_runner,
          IUpdaterInternalCallbackPtr(callback)));
  return S_OK;
}

}  // namespace updater
