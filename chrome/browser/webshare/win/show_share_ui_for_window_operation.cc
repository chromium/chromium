// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/show_share_ui_for_window_operation.h"

#include <EventToken.h>
#include <shlobj.h>
#include <windows.applicationmodel.datatransfer.h>
#include <wrl/event.h>

#include <utility>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
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
  ComPtr<IDataTransferManager> data_transfer_manager;
  HRESULT hr = GetDataTransferManagerHandles(
      hwnd_, &data_transfer_manager_interop, &data_transfer_manager);
  if (FAILED(hr))
    return Cancel();

  // Create and register a data requested handler
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
  EventRegistrationToken data_requested_token{};
  hr = data_transfer_manager->add_DataRequested(
      raw_data_requested_callback.Get(), &data_requested_token);

  // Create a callback to clean up the data requested handler that doesn't rely
  // on |this| so it can still be run even if |this| has been destroyed
  auto remove_data_requested_listener = base::BindOnce(
      [](ComPtr<IDataTransferManager> data_transfer_manager,
         EventRegistrationToken data_requested_token) {
        if (data_transfer_manager && data_requested_token.value) {
          data_transfer_manager->remove_DataRequested(data_requested_token);
        }
      },
      data_transfer_manager, data_requested_token);

  // If the call to register the data requested handler failed, clean up
  // listener and cancel the operation
  if (FAILED(hr)) {
    std::move(remove_data_requested_listener).Run();
    return Cancel();
  }

  // Request showing the Share UI
  hr = data_transfer_manager_interop->ShowShareUIForWindow(hwnd_);

  // If the call is expected to complete later, save the clean-up callback for
  // later use and schedule a timeout to cover any cases where it fails (and
  // therefore never comes)
  if (SUCCEEDED(hr) && weak_ptr && data_requested_callback_) {
    remove_data_requested_listener_ = std::move(remove_data_requested_listener);
    if (!base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&ShowShareUIForWindowOperation::Cancel, weak_ptr),
            kMaxExecutionTime)) {
      return Cancel();
    }
  } else {
    // In all other cases (i.e. failure or synchronous completion), remove the
    // listener right away
    std::move(remove_data_requested_listener).Run();

    // In the failure case, also cancel the operation (if it was not already
    // cancelled synchronously).
    if (FAILED(hr) && weak_ptr)
      return Cancel();
  }
}

void ShowShareUIForWindowOperation::Cancel() {
  if (remove_data_requested_listener_)
    std::move(remove_data_requested_listener_).Run();

  if (data_requested_callback_)
    std::move(data_requested_callback_).Run(nullptr);
}

void ShowShareUIForWindowOperation::OnDataRequested(
    IDataTransferManager* data_transfer_manager,
    IDataRequestedEventArgs* event_args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the callback to remove the DataRequested listener has been stored on
  // |this| (i.e. this function is being invoked asynchronously), invoke it now
  // before invoking the |data_requested_callback_|, as that may result in
  // |this| being destroyed. Note that the callback to remove the DataRequested
  // listener will not have been set if this is being invoked synchronously as
  // part of the ShowShareUIForWindow call, as the system APIs don't handle
  // unregistering at that point.
  if (remove_data_requested_listener_)
    std::move(remove_data_requested_listener_).Run();

  std::move(data_requested_callback_).Run(event_args);
}

}  // namespace webshare
