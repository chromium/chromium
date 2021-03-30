// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/com_classes.h"

#include <wchar.h>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/app/server/win/server.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_version.h"

namespace updater {

STDMETHODIMP UpdateStateImpl::get_state(LONG* state) {
  DCHECK(state);
  *state = static_cast<LONG>(update_state_.state);
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_appId(BSTR* app_id) {
  DCHECK(app_id);
  *app_id =
      base::win::ScopedBstr(base::UTF8ToWide(update_state_.app_id)).Release();
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_nextVersion(BSTR* next_version) {
  DCHECK(next_version);
  *next_version =
      base::win::ScopedBstr(
          update_state_.next_version.IsValid()
              ? base::UTF8ToWide(update_state_.next_version.GetString())
              : L"")
          .Release();
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_downloadedBytes(LONGLONG* downloaded_bytes) {
  DCHECK(downloaded_bytes);
  *downloaded_bytes = LONGLONG{update_state_.downloaded_bytes};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_totalBytes(LONGLONG* total_bytes) {
  DCHECK(total_bytes);
  *total_bytes = LONGLONG{update_state_.total_bytes};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_installProgress(LONG* install_progress) {
  DCHECK(install_progress);
  *install_progress = LONG{update_state_.install_progress};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_errorCategory(LONG* error_category) {
  DCHECK(error_category);
  *error_category = static_cast<LONG>(update_state_.error_category);
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_errorCode(LONG* error_code) {
  DCHECK(error_code);
  *error_code = LONG{update_state_.error_code};
  return S_OK;
}

STDMETHODIMP UpdateStateImpl::get_extraCode1(LONG* extra_code1) {
  DCHECK(extra_code1);
  *extra_code1 = LONG{update_state_.extra_code1};
  return S_OK;
}

STDMETHODIMP CompleteStatusImpl::get_statusCode(LONG* code) {
  DCHECK(code);
  *code = code_;
  return S_OK;
}

STDMETHODIMP CompleteStatusImpl::get_statusMessage(BSTR* message) {
  DCHECK(message);
  *message = base::win::ScopedBstr(message_).Release();
  return S_OK;
}

HRESULT UpdaterImpl::GetVersion(BSTR* version) {
  DCHECK(version);

  // Return the hardcoded version instead of calling the corresponding
  // non-blocking function of `UpdateServiceImpl`. This results in some
  // code duplication but it avoids the complexities of making this function
  // non-blocking.
  *version =
      base::win::ScopedBstr(
          base::UTF8ToWide(base::Version(UPDATER_VERSION_STRING).GetString()))
          .Release();
  return S_OK;
}

HRESULT UpdaterImpl::CheckForUpdate(const wchar_t* app_id) {
  return E_NOTIMPL;
}

HRESULT UpdaterImpl::RegisterApp(const wchar_t* app_id,
                                 const wchar_t* brand_code,
                                 const wchar_t* tag,
                                 const wchar_t* version,
                                 const wchar_t* existence_checker_path,
                                 IUpdaterRegisterAppCallback* callback) {
  if (!callback)
    return E_INVALIDARG;

  // Validates that string parameters are not longer than 16K characters.
  base::Optional<RegistrationRequest> request =
      [app_id, brand_code, tag, version,
       existence_checker_path]() -> decltype(request) {
    for (const auto* str :
         {app_id, brand_code, tag, version, existence_checker_path}) {
      constexpr size_t kMaxStringLen = 0x4000;  // 16KB.
      if (wcsnlen_s(str, kMaxStringLen) == kMaxStringLen) {
        return base::nullopt;
      }
    }

    RegistrationRequest request;
    if (!app_id || !base::WideToUTF8(app_id, wcslen(app_id), &request.app_id)) {
      return base::nullopt;
    }
    if (!brand_code || !base::WideToUTF8(brand_code, wcslen(brand_code),
                                         &request.brand_code)) {
      return base::nullopt;
    }
    if (!tag || !base::WideToUTF8(tag, wcslen(tag), &request.tag)) {
      return base::nullopt;
    }
    std::string version_str;
    if (!version || !base::WideToUTF8(version, wcslen(version), &version_str)) {
      return base::nullopt;
    }
    request.version = base::Version(version_str);
    if (!request.version.IsValid()) {
      return base::nullopt;
    }
    request.existence_checker_path = base::FilePath(existence_checker_path);

    return request;
  }();

  if (!request)
    return E_INVALIDARG;

  using IUpdaterRegisterAppCallbackPtr =
      Microsoft::WRL::ComPtr<IUpdaterRegisterAppCallback>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();

  // This task runner is responsible for sequencing the COM calls and callbacks.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const RegistrationRequest& request,
             IUpdaterRegisterAppCallbackPtr callback) {
            update_service->RegisterApp(
                request,
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterRegisterAppCallbackPtr callback,
                       const RegistrationResponse& response) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(&IUpdaterRegisterAppCallback::Run,
                                         callback, response.status_code),
                          base::BindOnce([](HRESULT hr) {
                            DVLOG(2) << "UpdaterImpl::RegisterApp "
                                     << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, callback));
          },
          com_server->update_service(), task_runner, *request,
          IUpdaterRegisterAppCallbackPtr(callback)));

  return S_OK;
}

// Called by the COM RPC runtime on one of its threads. Invokes the in-process
// `update_service` on the main sequence. The callbacks received from
// `update_service` arrive in the main sequence too.
HRESULT UpdaterImpl::RunPeriodicTasks(IUpdaterCallback* callback) {
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();
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

// Called by the COM RPC runtime on one of its threads. Invokes the in-process
// `update_service` on the main sequence. The callbacks received from
// `update_service` arrive in the main sequence too. Since handling these
// callbacks involves issuing outgoing COM RPC calls, which block, such COM
// calls must be done through a task runner, bound to the closures provided
// as parameters for the UpdateService::Update call.
HRESULT UpdaterImpl::Update(const wchar_t* app_id, IUpdaterObserver* observer) {
  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();

  // This task runner is responsible for sequencing the callbacks posted
  // by the `UpdateService` and calling the outbound COM functions to
  // notify the client about state changes in the `UpdateService`.
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             const std::string app_id, IUpdaterObserverPtr observer) {
            update_service->Update(
                app_id, UpdateService::Priority::kForeground,
                base::BindRepeating(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterObserverPtr observer,
                       UpdateService::UpdateState update_state) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(&IUpdaterObserver::OnStateChange,
                                         observer,
                                         Microsoft::WRL::Make<UpdateStateImpl>(
                                             update_state)),
                          base::BindOnce([](HRESULT hr) {
                            DVLOG(4)
                                << "IUpdaterObserver::OnStateChange returned "
                                << std::hex << hr;
                          }));
                    },
                    task_runner, observer),
                base::BindOnce(
                    [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                       IUpdaterObserverPtr observer,
                       UpdateService::Result result) {
                      task_runner->PostTaskAndReplyWithResult(
                          FROM_HERE,
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              Microsoft::WRL::Make<CompleteStatusImpl>(
                                  static_cast<int>(result), L"")),
                          base::BindOnce([](HRESULT hr) {
                            DVLOG(2) << "UpdaterImpl::Update "
                                     << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, observer));
          },
          com_server->update_service(), task_runner, base::WideToUTF8(app_id),
          IUpdaterObserverPtr(observer)));

  // Always return S_OK from this function. Errors must be reported using the
  // observer interface.
  return S_OK;
}

// See the comment for the UpdaterImpl::Update.
HRESULT UpdaterImpl::UpdateAll(IUpdaterObserver* observer) {
  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();

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
                              Microsoft::WRL::Make<CompleteStatusImpl>(
                                  static_cast<int>(result), L"")),
                          base::BindOnce([](HRESULT hr) {
                            DVLOG(2) << "UpdaterImpl::UpdateAll "
                                     << "callback returned " << std::hex << hr;
                          }));
                    },
                    task_runner, observer));
          },
          com_server->update_service(), task_runner,
          IUpdaterObserverPtr(observer)));

  // Always return S_OK from this function. Errors must be reported using the
  // observer interface.
  return S_OK;
}

// See the comment for the UpdaterImpl::Update.
HRESULT UpdaterInternalImpl::Run(IUpdaterInternalCallback* callback) {
  using IUpdaterInternalCallbackPtr =
      Microsoft::WRL::ComPtr<IUpdaterInternalCallback>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();

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
                        DVLOG(2) << "UpdaterInternalImpl::Run "
                                 << "callback returned " << std::hex << hr;
                      }));
                },
                task_runner, callback));
          },
          com_server->update_service_internal(), task_runner,
          IUpdaterInternalCallbackPtr(callback)));

  // Always return S_OK from this function. Errors must be reported using the
  // callback interface.
  return S_OK;
}

HRESULT UpdaterInternalImpl::InitializeUpdateService(
    IUpdaterInternalCallback* callback) {
  using IUpdaterInternalCallbackPtr =
      Microsoft::WRL::ComPtr<IUpdaterInternalCallback>;
  scoped_refptr<ComServerApp> com_server = AppServerSingletonInstance();

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateServiceInternal> update_service_internal,
             scoped_refptr<base::SequencedTaskRunner> task_runner,
             IUpdaterInternalCallbackPtr callback) {
            update_service_internal->InitializeUpdateService(base::BindOnce(
                [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                   IUpdaterInternalCallbackPtr callback) {
                  task_runner->PostTaskAndReplyWithResult(
                      FROM_HERE,
                      base::BindOnce(&IUpdaterInternalCallback::Run, callback,
                                     0),
                      base::BindOnce([](HRESULT hr) {
                        DVLOG(2)
                            << "UpdaterInternalImpl::InitializeUpdateService "
                            << "callback returned " << std::hex << hr;
                      }));
                },
                task_runner, callback));
          },
          com_server->update_service_internal(), task_runner,
          IUpdaterInternalCallbackPtr(callback)));

  // Always return S_OK from this function. Errors must be reported using the
  // callback interface.
  return S_OK;
}

}  // namespace updater
