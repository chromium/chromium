// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/show_share_ui_for_window_operation.h"

#include <shlobj.h>
#include <windows.applicationmodel.datatransfer.h>
#include <wrl/event.h>

#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/core_winrt_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using ABI::Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace webshare {
namespace {

decltype(
    &base::win::RoGetActivationFactory) ro_get_activation_factory_function_ =
    &base::win::RoGetActivationFactory;

// Fetches handles to the IDataTransferManager[Interop] instances for the
// given |hwnd|
HRESULT GetDataTransferManagerHandles(
    HWND hwnd,
    IDataTransferManagerInterop** data_transfer_manager_interop,
    IDataTransferManager** data_transfer_manager) {
  // If the required WinRT functionality is not available, fail the operation
  if (!base::win::ResolveCoreWinRTDelayload() ||
      !base::win::ScopedHString::ResolveCoreWinRTStringDelayload()) {
    return E_FAIL;
  }

  // IDataTransferManagerInterop is semi-hidden behind a CloakedIid
  // structure on the DataTransferManager, excluding it from things
  // used by RoGetActivationFactory like GetIids(). Because of this,
  // the safe way to fetch a pointer to it is through a publicly
  // supported IID (e.g. IUnknown), followed by a QueryInterface call
  // (or something that simply wraps it like As()) to convert it.
  auto class_id_hstring = base::win::ScopedHString::Create(
      RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager);
  if (!class_id_hstring.is_valid())
    return E_FAIL;

  ComPtr<IUnknown> data_transfer_manager_factory;
  HRESULT hr = ro_get_activation_factory_function_(
      class_id_hstring.get(), IID_PPV_ARGS(&data_transfer_manager_factory));
  if (FAILED(hr))
    return hr;

  hr = data_transfer_manager_factory->QueryInterface(
      data_transfer_manager_interop);
  if (FAILED(hr))
    return hr;

  hr = (*data_transfer_manager_interop)
           ->GetForWindow(hwnd, IID_PPV_ARGS(data_transfer_manager));
  return hr;
}
}  // namespace

ShowShareUIForWindowOperation::ShowShareUIForWindowOperation(HWND hwnd)
    : hwnd_(hwnd) {
  data_requested_token_.value = 0;
}

ShowShareUIForWindowOperation::~ShowShareUIForWindowOperation() {
  Cancel();
}

// static
void ShowShareUIForWindowOperation::SetRoGetActivationFactoryFunctionForTesting(
    decltype(&base::win::RoGetActivationFactory) value) {
  ro_get_activation_factory_function_ = value;
}

void ShowShareUIForWindowOperation::Run(
    DataRequestedCallback data_requested_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  data_requested_callback_ = std::move(data_requested_callback);

  // Fetch the OS handles needed
  ComPtr<IDataTransferManagerInterop> data_transfer_manager_interop;
  HRESULT hr = GetDataTransferManagerHandles(
      hwnd_, &data_transfer_manager_interop, &data_transfer_manager_);
  if (FAILED(hr))
    return Cancel();

  // Create and register a data request handler
  auto weak_ptr = weak_factory_.GetWeakPtr();
  auto raw_data_requested_callback = Callback<
      ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs*>>(
      [weak_ptr](IDataTransferManager* data_transfer_manager,
                 IDataRequestedEventArgs* event_args) -> HRESULT {
        DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
        if (weak_ptr)
          weak_ptr.get()->OnDataRequested(data_transfer_manager, event_args);

        // Always return S_OK, as returning a FAILED value results in the OS
        // killing this process. If the data population failed the OS Share
        // operation will fail gracefully with messaging to the user.
        return S_OK;
      });
  hr = data_transfer_manager_->add_DataRequested(
      raw_data_requested_callback.Get(), &data_requested_token_);
  if (FAILED(hr))
    return Cancel();

  // Request showing the Share UI
  show_share_ui_for_window_call_in_progress_ = true;
  hr = data_transfer_manager_interop->ShowShareUIForWindow(hwnd_);
  show_share_ui_for_window_call_in_progress_ = false;

  // If the call is expected to complete later, schedule a timeout to cover
  // any cases where it fails (and therefore never comes)
  if (SUCCEEDED(hr) && data_requested_callback_) {
    if (!base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ShowShareUIForWindowOperation::Cancel,
                           weak_factory_.GetWeakPtr()),
            kMaxExecutionTime)) {
      return Cancel();
    }
  } else {
    RemoveDataRequestedListener();
  }
}

void ShowShareUIForWindowOperation::Cancel() {
  RemoveDataRequestedListener();
  if (data_requested_callback_) {
    std::move(data_requested_callback_).Run(nullptr);
  }
}

void ShowShareUIForWindowOperation::OnDataRequested(
    IDataTransferManager* data_transfer_manager,
    IDataRequestedEventArgs* event_args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(data_transfer_manager, data_transfer_manager_.Get());

  // Remove the DataRequested handler if this is being invoked asynchronously.
  // If this is an in-progress invocation the system APIs don't handle the
  // event being unregistered while it is being executed, but we will unregister
  // it after the ShowShareUIForWindow call completes.
  if (!show_share_ui_for_window_call_in_progress_)
    RemoveDataRequestedListener();

  std::move(data_requested_callback_).Run(event_args);
}

void ShowShareUIForWindowOperation::RemoveDataRequestedListener() {
  if (data_transfer_manager_ && data_requested_token_.value) {
    data_transfer_manager_->remove_DataRequested(data_requested_token_);
    data_requested_token_.value = 0;
  }
}

}  // namespace webshare
